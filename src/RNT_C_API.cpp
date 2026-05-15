#include "RNT_C_API.h"

#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "Merkle.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
#include "InMemoryBackend.h"
#include "SqliteBackend.h"
#include "TupleCodec.h"
#include "VM.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Global runtime state
//
// A single Runtime instance is created per process on the first successful
// rnt_init call.  All managers share this instance; they do not hold their
// own state.  Initialisation is protected by g_init_mutex so that concurrent
// first-callers are safe; once g_init_done is true the mutex is never
// contended again.
// ---------------------------------------------------------------------------

namespace
{
    struct Runtime
    {
        std::unique_ptr<nt::IStorageBackend> storage;
        nt::ObjectManager      objects;
        nt::PermissionsManager permissions;
        nt::IdentityManager    identities;
        nt::LifecycleManager   lifecycles;
        std::unique_ptr<nt::HandlerManager> handler;
      std::unique_ptr<nt::CursorManager>  cursors;
    };

    static std::unique_ptr<Runtime> g_rt;
    static bool                     g_init_done = false;

    bool is_initialized() { return g_init_done; }

    static std::vector<std::string> split_path(const char* path)
    {
        std::vector<std::string> parts;
        if (!path) return parts;
        std::string s(path);
        if (!s.empty() && s[0] == '/') s.erase(0, 1);
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part, '/'))
            if (!part.empty()) parts.push_back(part);
        return parts;
    }

    static std::vector<nt::Attribute> parse_kv(const char* kv)
    {
        std::vector<nt::Attribute> attrs;
        if (!kv) return attrs;
        std::stringstream ss(kv);
        std::string line;
        while (std::getline(ss, line))
        {
            if (line.empty()) continue;
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            attrs.push_back({ line.substr(0, pos), line.substr(pos + 1) });
        }
        return attrs;
    }

    static std::string tuple_to_kv(nt::Tuple* t)
    {
        std::string out;
        t->Reset();
        const nt::Attribute* a = t->Next();
        while (a)
        {
            out += a->name + "=" + a->value + "\n";
            a = t->Next();
        }
        return out;
    }

    static char* heap_str(const std::string& s)
    {
        char* p = new char[s.size() + 1];
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    }

    static std::unique_ptr<nt::ObjectManager::object_type> make_relation_type()
    {
        auto t      = std::make_unique<nt::ObjectManager::object_type>();
        t->label    = RELATION;
        t->disposable = false;
        t->methods  = { OPEN, CLOSE };
        t->exclusive = false;
        return t;
    }

    static std::unique_ptr<nt::ObjectManager::object_type> make_branch_type()
    {
        auto t      = std::make_unique<nt::ObjectManager::object_type>();
        t->label    = BRANCH;
        t->disposable = false;
        t->methods  = { OPEN, CLOSE };
        // Branches are mutable reference heads: serialize writers through
        // LifecycleManager::Contention.
        t->exclusive = true;
        return t;
    }

    // -----------------------------------------------------------------------
    // Plan builder internals
    // -----------------------------------------------------------------------

    /**
     * Wraps an entire plan subtree.  All PlanNode allocations and all
     * CursorManager::cursor* / HandlerManager::handle* opened by SCAN nodes
     * are collected here so they can be released atomically when the plan is
     * freed or transferred to a VmCursor.
     */
    struct PlanWrapper
    {
        nt::PlanNode*                                root     = nullptr;
        std::vector<std::unique_ptr<nt::PlanNode>>   nodes;    // owns every node in subtree
        std::vector<nt::CursorManager::cursor*>      cursors;  // one per SCAN leaf
        std::vector<nt::HandlerManager::handle*>     handles;  // one per SCAN leaf
    };

    static void free_plan_wrapper(PlanWrapper* pw)
    {
        if (!pw) return;
        for (auto* c : pw->cursors)  g_rt->cursors->Close(c);
        for (auto* h : pw->handles)  g_rt->handler->Close(h);
        delete pw;
    }

    /**
     * A VM cursor owns the materialised plan tree and all resources opened
     * during plan construction.  After rnt_vm_execute_plan the caller drives
     * it with rnt_vm_cursor_next / rnt_vm_cursor_close.
     */
    struct VmCursor
    {
        nt::VM                                       vm;
        nt::PlanNode*                                root;
        std::vector<std::unique_ptr<nt::PlanNode>>   nodes;
        std::vector<nt::CursorManager::cursor*>      cursors;
        std::vector<nt::HandlerManager::handle*>     handles;

        explicit VmCursor(nt::CursorManager& cm, PlanWrapper* pw)
            : vm(cm), root(pw->root)
        {
            nodes   = std::move(pw->nodes);
            cursors = std::move(pw->cursors);
            handles = std::move(pw->handles);
            delete pw;  // struct itself; resources transferred above
        }

        ~VmCursor()
        {
            for (auto* c : cursors) g_rt->cursors->Close(c);
            for (auto* h : handles) g_rt->handler->Close(h);
        }
    };
}

// ---------------------------------------------------------------------------
// Public C API
//
// Section order mirrors the NT open-handle pipeline:
//   Runtime lifecycle → Authentication → Handle lifecycle →
//   Object registration → Tuple storage → Cursor / VM plan builder
// ---------------------------------------------------------------------------

int rnt_init(const char* driver, const char* storage_path)
{
    try
    {
        auto rt = std::make_unique<Runtime>();

        const std::string drv = driver ? driver : "sqlite";
        if (drv == "memory")
            rt->storage = std::make_unique<nt::InMemoryBackend>();
        else
            rt->storage = std::make_unique<nt::SqliteBackend>(
                storage_path ? storage_path : ":memory:");

        rt->handler = std::make_unique<nt::HandlerManager>(
            rt->objects, rt->permissions, rt->identities, rt->lifecycles);
        rt->cursors = std::make_unique<nt::CursorManager>(*rt->storage);
        g_rt = std::move(rt);

        // Register the default branch on first boot so that
        // rnt_open_handle("/system/branches/master") always succeeds.
        const auto master_path = split_path("/system/branches/master");
        if (g_rt->objects.Find(master_path) == nullptr)
        {
            auto branch  = std::make_unique<nt::ObjectManager::Branch>();
            branch->name = "master";
            g_rt->objects.Register(master_path, std::move(branch),
                                   make_branch_type());
        }

        g_init_done = true;
        return 0;
    }
    catch (...)
    {
        g_rt.reset();
        return -1;
    }
}

int rnt_firewall(const char* /*auth_method*/, char** claims_out)
{
    if (!is_initialized() || !claims_out) return -1;
    // PermissionsManager::Firewall is stubbed; grant all claims for now.
    *claims_out = heap_str("READ WRITE");
    return 0;
}

rnt_handle_t rnt_open_handle(const char* path, const char* /*claims*/)
{
    if (!is_initialized() || !path) return nullptr;
    return g_rt->handler->Open(split_path(path), nullptr);
}

int rnt_close_handle(rnt_handle_t handle)
{
    if (!is_initialized() || !handle) return -1;
    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    return g_rt->handler->Close(h) ? 0 : -1;
}

int rnt_branch_payload(rnt_handle_t handle, uint8_t** payload_out, size_t* len_out)
{
    if (!is_initialized() || !handle || !payload_out || !len_out) return -1;

    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    if (!h->object || !h->object->head) return -1;
    if (h->object->head->type->label != BRANCH) return -1;

    auto* branch = dynamic_cast<nt::ObjectManager::Branch*>(h->object->object.get());
    if (!branch) return -1;

    const auto& src = branch->payload;
    auto* buf = new uint8_t[src.size()];
    std::memcpy(buf, src.data(), src.size());
    *payload_out = buf;
    *len_out     = src.size();
    return 0;
}

int rnt_branch_set_payload(rnt_handle_t handle, const uint8_t* payload, size_t len)
{
    if (!is_initialized() || !handle) return -1;

    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    if (!h->object || !h->object->head) return -1;
    if (h->object->head->type->label != BRANCH) return -1;

    auto* branch = dynamic_cast<nt::ObjectManager::Branch*>(h->object->object.get());
    if (!branch) return -1;

    branch->payload.assign(payload, payload + len);
    return 0;
}

int rnt_register_relation(const char* path)
{
    if (!is_initialized() || !path) return -1;
    const auto parts = split_path(path);
    if (g_rt->objects.Find(parts) != nullptr) return 0;
    g_rt->objects.Register(
        parts,
        std::make_unique<nt::ObjectManager::Relation>(),
        make_relation_type());
    return 0;
}

int rnt_register_branch(const char* path, const uint8_t* payload, size_t payload_len)
{
    if (!is_initialized() || !path) return -1;
    const auto parts = split_path(path);
    if (g_rt->objects.Find(parts) != nullptr) return 0;

    auto branch  = std::make_unique<nt::ObjectManager::Branch>();
    branch->name = parts.empty() ? "" : parts.back();
    if (payload && payload_len > 0)
        branch->payload.assign(payload, payload + payload_len);

    g_rt->objects.Register(parts, std::move(branch), make_branch_type());
    return 0;
}

// Looks up the Relation object for a path; returns nullptr when not found or wrong type.
static nt::ObjectManager::Relation* find_relation(const std::vector<std::string>& parts)
{
    auto* entry = g_rt->objects.Find(parts);
    if (!entry || !entry->object) return nullptr;
    return dynamic_cast<nt::ObjectManager::Relation*>(entry->object.get());
}

int rnt_link_tuple(const char* relation_path, const char* kv_attrs, char** hash_out)
{
    if (!is_initialized() || !relation_path || !kv_attrs || !hash_out) return -1;
    const auto parts = split_path(relation_path);

    auto* rel = find_relation(parts);
    if (!rel) return -1;

    auto bytes      = nt::TupleCodec::Serialize(parse_kv(kv_attrs));
    const auto hash = g_rt->storage->Put(std::move(bytes));

    rel->merkle_root = nt::Merkle::Insert(*g_rt->storage, rel->merkle_root, hash);

    *hash_out = heap_str(hash);
    return 0;
}

int rnt_unlink_tuple(const char* relation_path, const char* tuple_hash)
{
    if (!is_initialized() || !relation_path || !tuple_hash) return -1;
    auto* rel = find_relation(split_path(relation_path));
    if (!rel) return -1;
    rel->merkle_root = nt::Merkle::Remove(*g_rt->storage, rel->merkle_root, tuple_hash);
    return 0;
}

int rnt_clear_relation(const char* relation_path)
{
    if (!is_initialized() || !relation_path) return -1;
    auto* rel = find_relation(split_path(relation_path));
    if (!rel) return -1;
    rel->merkle_root.clear();
    return 0;
}

int rnt_relation_root(const char* relation_path, char** root_hash_out)
{
    if (!is_initialized() || !relation_path || !root_hash_out) return -1;
    auto* rel = find_relation(split_path(relation_path));
    if (!rel) return -1;
    *root_hash_out = heap_str(rel->merkle_root);
    return 0;
}

int rnt_set_relation_root(const char* relation_path, const char* root_hash)
{
    if (!is_initialized() || !relation_path) return -1;
    auto* rel = find_relation(split_path(relation_path));
    if (!rel) return -1;
    rel->merkle_root = root_hash ? root_hash : "";
    return 0;
}

rnt_cursor_t rnt_cursor_open(rnt_handle_t handle)
{
    if (!is_initialized() || !handle) return nullptr;
    auto* h = static_cast<nt::HandlerManager::handle*>(handle);

    std::string merkle_root;
    if (h->object && h->object->head &&
        h->object->head->type->label == RELATION)
    {
        auto* rel = find_relation(h->object->head->path);
        if (rel) merkle_root = rel->merkle_root;
    }

    return g_rt->cursors->Open(h, merkle_root);
}

int rnt_cursor_next(rnt_cursor_t cursor, char** tuple_out)
{
    if (!is_initialized() || !cursor || !tuple_out) return -1;
    auto* c = static_cast<nt::CursorManager::cursor*>(cursor);
    nt::Tuple* t = g_rt->cursors->Next(c);
    if (!t) { *tuple_out = nullptr; return 0; }
    *tuple_out = heap_str(tuple_to_kv(t));
    return 1;
}

int rnt_cursor_close(rnt_cursor_t cursor)
{
    if (!is_initialized() || !cursor) return -1;
    g_rt->cursors->Close(static_cast<nt::CursorManager::cursor*>(cursor));
    return 0;
}

void rnt_free_string(char* s)  { delete[] s; }
void rnt_free_bytes(uint8_t* p) { delete[] p; }

// ---------------------------------------------------------------------------
// VM plan builder
// ---------------------------------------------------------------------------

rnt_plan_t rnt_plan_scan(const char* relation_path)
{
    if (!is_initialized() || !relation_path) return nullptr;

    // Open a handle to the stored relation through the full manager pipeline.
    const auto parts = split_path(relation_path);
    auto* h = g_rt->handler->Open(parts, nullptr);
    if (!h) return nullptr;

    // Seed the cursor with the current Merkle root from the ObjectManager.
    std::string merkle_root;
    auto* rel = find_relation(parts);
    if (rel) merkle_root = rel->merkle_root;

    auto* c = g_rt->cursors->Open(h, merkle_root);
    if (!c)
    {
        g_rt->handler->Close(h);
        return nullptr;
    }

    auto  node        = std::make_unique<nt::PlanNode>();
    node->op          = nt::PlanNode::Op::SCAN;
    node->scan_cursor = c;

    auto* pw       = new PlanWrapper();
    pw->root       = node.get();
    pw->nodes.push_back(std::move(node));
    pw->cursors.push_back(c);
    pw->handles.push_back(h);
    return pw;
}

rnt_plan_t rnt_plan_join(rnt_plan_t left, rnt_plan_t right)
{
    if (!left || !right)
    {
        free_plan_wrapper(static_cast<PlanWrapper*>(left));
        free_plan_wrapper(static_cast<PlanWrapper*>(right));
        return nullptr;
    }

    auto* l = static_cast<PlanWrapper*>(left);
    auto* r = static_cast<PlanWrapper*>(right);

    auto  node  = std::make_unique<nt::PlanNode>();
    node->op    = nt::PlanNode::Op::JOIN;
    node->left  = l->root;
    node->right = r->root;

    auto* pw  = new PlanWrapper();
    pw->root  = node.get();
    pw->nodes.push_back(std::move(node));

    // Absorb child resources into the new wrapper.
    for (auto& n : l->nodes)   pw->nodes.push_back(std::move(n));
    for (auto& n : r->nodes)   pw->nodes.push_back(std::move(n));
    for (auto* c : l->cursors) pw->cursors.push_back(c);
    for (auto* c : r->cursors) pw->cursors.push_back(c);
    for (auto* h : l->handles) pw->handles.push_back(h);
    for (auto* h : r->handles) pw->handles.push_back(h);

    delete l;
    delete r;
    return pw;
}

rnt_plan_t rnt_plan_take(rnt_plan_t source, size_t limit)
{
    if (!source) return nullptr;

    auto* s = static_cast<PlanWrapper*>(source);

    auto  node         = std::make_unique<nt::PlanNode>();
    node->op           = nt::PlanNode::Op::TAKE;
    node->left         = s->root;
    node->take_limit   = limit;

    auto* pw  = new PlanWrapper();
    pw->root  = node.get();
    pw->nodes.push_back(std::move(node));

    for (auto& n : s->nodes)   pw->nodes.push_back(std::move(n));
    for (auto* c : s->cursors) pw->cursors.push_back(c);
    for (auto* h : s->handles) pw->handles.push_back(h);

    delete s;
    return pw;
}

void rnt_plan_free(rnt_plan_t plan)
{
    free_plan_wrapper(static_cast<PlanWrapper*>(plan));
}

rnt_cursor_t rnt_vm_execute_plan(rnt_plan_t plan)
{
    if (!is_initialized() || !plan) return nullptr;
    // VmCursor constructor transfers ownership from PlanWrapper and deletes it.
    return new VmCursor(*g_rt->cursors, static_cast<PlanWrapper*>(plan));
}

int rnt_vm_cursor_next(rnt_cursor_t vm_cursor, char** tuple_out)
{
    if (!is_initialized() || !vm_cursor || !tuple_out) return -1;
    auto* vc = static_cast<VmCursor*>(vm_cursor);
    nt::Tuple* t = vc->vm.Next(vc->root);
    if (!t) { *tuple_out = nullptr; return 0; }
    *tuple_out = heap_str(tuple_to_kv(t));
    return 1;
}

int rnt_vm_cursor_close(rnt_cursor_t vm_cursor)
{
    if (!is_initialized() || !vm_cursor) return -1;
    // VmCursor destructor closes all cursors and handles.
    delete static_cast<VmCursor*>(vm_cursor);
    return 0;
}
