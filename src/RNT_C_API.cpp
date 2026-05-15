#include "RNT_C_API.h"

#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "Merkle.h"
#include "MultigroupCodec.h"
#include "NamespaceReferenceManager.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
#include "InMemoryBackend.h"
#include "SqliteBackend.h"
#include "TupleCodec.h"
#include "VM.h"

#include <algorithm>
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
        std::unique_ptr<nt::NamespaceReferenceManager> references;
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

    static std::unique_ptr<nt::ObjectManager::object_type> make_multigroup_type()
    {
        auto t      = std::make_unique<nt::ObjectManager::object_type>();
        t->label    = MULTIGROUP;
        t->disposable = false;
        t->methods  = { OPEN, CLOSE };
        // Multigroup snapshots are immutable and content-addressed; concurrent
        // readers never contend. GC is gated by reference_count only.
        t->exclusive = false;
        return t;
    }

    // Serializes the (name, merkle_root) pairs into the storage backend and
    // registers the resulting snapshot under /system/snapshots/<hash>, along
    // with one Relation entry per child at /system/snapshots/<hash>/relations/<n>.
    // Idempotent: if the snapshot already exists in the registry the existing
    // entries are reused. Returns the multigroup hash, or empty string on error.
    //
    // Child Relation entries are immutable: their merkle_root is fixed at
    // snapshot creation time. Cursors opened against them read the relation's
    // Merkle B-tree at the exact root the snapshot recorded.
    static std::string register_snapshot(
        const std::vector<nt::MultigroupCodec::RelationEntry>& relations)
    {
        if (!g_rt) return {};

        const std::string hash = nt::MultigroupCodec::Store(*g_rt->storage, relations);
        const auto snap_path = split_path(("/system/snapshots/" + hash).c_str());

        if (g_rt->objects.Find(snap_path) != nullptr) return hash;

        auto mg = std::make_unique<nt::ObjectManager::Multigroup>();
        mg->merkle_root = hash;
        g_rt->objects.Register(snap_path, std::move(mg), make_multigroup_type());

        for (const auto& [name, root] : relations)
        {
            const auto rel_path = split_path(
                ("/system/snapshots/" + hash + "/relations/" + name).c_str());
            if (g_rt->objects.Find(rel_path) != nullptr) continue;

            auto rel = std::make_unique<nt::ObjectManager::Relation>();
            rel->merkle_root = root;
            g_rt->objects.Register(rel_path, std::move(rel), make_relation_type());
        }
        return hash;
    }

    // Reads the current snapshot's (name, root) list for a branch. Returns an
    // empty vector for unborn branches or any lookup failure.
    static std::vector<nt::MultigroupCodec::RelationEntry> read_branch_relations(
        const nt::ObjectManager::Branch& branch)
    {
        if (branch.target_hash.empty()) return {};

        const auto snap_path = split_path(
            ("/system/snapshots/" + branch.target_hash).c_str());
        auto* entry = g_rt->objects.Find(snap_path);
        if (!entry || !entry->object) return {};

        auto* mg = dynamic_cast<nt::ObjectManager::Multigroup*>(entry->object.get());
        if (!mg) return {};

        auto opt = g_rt->storage->Get(mg->merkle_root);
        if (!opt) return {};
        return nt::MultigroupCodec::Deserialize(*opt);
    }

    // Looks up a branch object by path. Returns nullptr when the path does not
    // resolve to a BRANCH.
    static nt::ObjectManager::Branch* find_branch(
        const std::vector<std::string>& branch_path)
    {
        auto* entry = g_rt->objects.Find(branch_path);
        if (!entry || !entry->object || !entry->head) return nullptr;
        if (entry->head->type->label != BRANCH) return nullptr;
        return dynamic_cast<nt::ObjectManager::Branch*>(entry->object.get());
    }

    // Parses a path of the form /system/branches/<name>/relations/<relation_name>.
    // Returns true on success; out-parameters are populated with the branch path
    // and the relation name. Returns false for any other path shape.
    static bool split_branch_relation(const std::vector<std::string>& parts,
                                      std::vector<std::string>& branch_path_out,
                                      std::string& relation_name_out)
    {
        if (parts.size() != 5) return false;
        if (parts[0] != "system" || parts[1] != "branches" || parts[3] != "relations")
            return false;
        branch_path_out = { "system", "branches", parts[2] };
        relation_name_out = parts[4];
        return true;
    }

    // Applies a per-relation merkle_root update to the branch's current snapshot:
    // reads the existing (name, root) list, replaces or appends (relation_name,
    // new_root), registers the resulting snapshot, and advances the branch.
    // Returns the new snapshot hash, or empty string on error.
    static std::string commit_relation_update(
        nt::ObjectManager::Branch& branch,
        const std::string& relation_name,
        const std::string& new_root)
    {
        auto relations = read_branch_relations(branch);
        auto it = std::find_if(relations.begin(), relations.end(),
                               [&](const auto& e) { return e.first == relation_name; });
        if (it != relations.end()) it->second = new_root;
        else relations.emplace_back(relation_name, new_root);

        const std::string new_snapshot_hash = register_snapshot(relations);
        if (new_snapshot_hash.empty()) return {};
        branch.target_hash = new_snapshot_hash;
        return new_snapshot_hash;
    }

    // Returns the merkle_root of a named relation in the branch's current
    // snapshot. Empty string when the branch is unborn or the relation is
    // absent.
    static std::string read_relation_root(const nt::ObjectManager::Branch& branch,
                                          const std::string& relation_name)
    {
        const auto relations = read_branch_relations(branch);
        auto it = std::find_if(relations.begin(), relations.end(),
                               [&](const auto& e) { return e.first == relation_name; });
        return (it == relations.end()) ? std::string{} : it->second;
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

        rt->references = std::make_unique<nt::NamespaceReferenceManager>(rt->objects);
        rt->handler = std::make_unique<nt::HandlerManager>(
            rt->objects, rt->permissions, rt->identities, rt->lifecycles,
            *rt->references);
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

int rnt_branch_target(rnt_handle_t handle, char** target_hash_out)
{
    if (!is_initialized() || !handle || !target_hash_out) return -1;

    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    if (!h->object || !h->object->head) return -1;
    if (h->object->head->type->label != BRANCH) return -1;

    auto* branch = dynamic_cast<nt::ObjectManager::Branch*>(h->object->object.get());
    if (!branch) return -1;

    *target_hash_out = heap_str(branch->target_hash);
    return 0;
}

int rnt_branch_advance(const char* branch_path, const char* new_hash)
{
    if (!is_initialized() || !branch_path || !new_hash) return -1;

    auto* entry = g_rt->objects.Find(split_path(branch_path));
    if (!entry || !entry->object || !entry->head) return -1;
    if (entry->head->type->label != BRANCH) return -1;

    auto* branch = dynamic_cast<nt::ObjectManager::Branch*>(entry->object.get());
    if (!branch) return -1;

    const std::string hash(new_hash);
    if (!hash.empty())
    {
        const auto snap_path = split_path(("/system/snapshots/" + hash).c_str());
        if (g_rt->objects.Find(snap_path) == nullptr) return -1;
    }

    branch->target_hash = hash;
    return 0;
}

int rnt_register_relation(const char* path)
{
    if (!is_initialized() || !path) return -1;
    const auto parts = split_path(path);

    std::vector<std::string> branch_path;
    std::string relation_name;
    if (!split_branch_relation(parts, branch_path, relation_name)) return -1;

    auto* branch = find_branch(branch_path);
    if (!branch) return -1;

    // Idempotent: if the relation is already present in the branch's current
    // snapshot, the call succeeds without producing a new snapshot.
    const auto current = read_branch_relations(*branch);
    const bool already_present = std::any_of(
        current.begin(), current.end(),
        [&](const auto& e) { return e.first == relation_name; });
    if (already_present) return 0;

    // Empty merkle_root signals a relation with no tuples.
    return commit_relation_update(*branch, relation_name, "").empty() ? -1 : 0;
}

int rnt_register_branch(const char* path, const char* target_hash)
{
    if (!is_initialized() || !path) return -1;
    const auto parts = split_path(path);
    if (g_rt->objects.Find(parts) != nullptr) return 0;

    const std::string hash = target_hash ? target_hash : "";
    if (!hash.empty())
    {
        const auto snap_path = split_path(("/system/snapshots/" + hash).c_str());
        if (g_rt->objects.Find(snap_path) == nullptr) return -1;
    }

    auto branch         = std::make_unique<nt::ObjectManager::Branch>();
    branch->name        = parts.empty() ? "" : parts.back();
    branch->target_hash = hash;

    g_rt->objects.Register(parts, std::move(branch), make_branch_type());
    return 0;
}

int rnt_link_tuple(const char* relation_path, const char* kv_attrs, char** hash_out)
{
    if (!is_initialized() || !relation_path || !kv_attrs || !hash_out) return -1;
    const auto parts = split_path(relation_path);

    std::vector<std::string> branch_path;
    std::string relation_name;
    if (!split_branch_relation(parts, branch_path, relation_name)) return -1;

    auto* branch = find_branch(branch_path);
    if (!branch) return -1;

    const std::string old_root = read_relation_root(*branch, relation_name);

    auto bytes           = nt::TupleCodec::Serialize(parse_kv(kv_attrs));
    const auto tuple_hash = g_rt->storage->Put(std::move(bytes));
    const std::string new_root =
        nt::Merkle::Insert(*g_rt->storage, old_root, tuple_hash);

    if (commit_relation_update(*branch, relation_name, new_root).empty())
        return -1;

    *hash_out = heap_str(tuple_hash);
    return 0;
}

int rnt_unlink_tuple(const char* relation_path, const char* tuple_hash)
{
    if (!is_initialized() || !relation_path || !tuple_hash) return -1;
    const auto parts = split_path(relation_path);

    std::vector<std::string> branch_path;
    std::string relation_name;
    if (!split_branch_relation(parts, branch_path, relation_name)) return -1;

    auto* branch = find_branch(branch_path);
    if (!branch) return -1;

    const std::string old_root = read_relation_root(*branch, relation_name);
    const std::string new_root =
        nt::Merkle::Remove(*g_rt->storage, old_root, tuple_hash);

    return commit_relation_update(*branch, relation_name, new_root).empty()
        ? -1 : 0;
}

int rnt_clear_relation(const char* relation_path)
{
    if (!is_initialized() || !relation_path) return -1;
    const auto parts = split_path(relation_path);

    std::vector<std::string> branch_path;
    std::string relation_name;
    if (!split_branch_relation(parts, branch_path, relation_name)) return -1;

    auto* branch = find_branch(branch_path);
    if (!branch) return -1;

    return commit_relation_update(*branch, relation_name, "").empty() ? -1 : 0;
}

int rnt_relation_root(const char* relation_path, char** root_hash_out)
{
    if (!is_initialized() || !relation_path || !root_hash_out) return -1;
    const auto parts = split_path(relation_path);

    std::vector<std::string> branch_path;
    std::string relation_name;
    if (!split_branch_relation(parts, branch_path, relation_name)) return -1;

    auto* branch = find_branch(branch_path);
    if (!branch) return -1;

    *root_hash_out = heap_str(read_relation_root(*branch, relation_name));
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
        auto* rel = dynamic_cast<nt::ObjectManager::Relation*>(h->object->object.get());
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
    // Open() runs NamespaceReferenceManager::Resolve internally, so the handle
    // lands on the resolved /system/snapshots/<hash>/relations/<n> entry when
    // the caller passed a branch-relative path.
    const auto parts = split_path(relation_path);
    auto* h = g_rt->handler->Open(parts, nullptr);
    if (!h) return nullptr;

    // Read the Merkle root straight from the handle's object — Resolve has
    // already pointed it at the snapshot-bound Relation entry.
    std::string merkle_root;
    if (h->object && h->object->object)
    {
        auto* rel = dynamic_cast<nt::ObjectManager::Relation*>(h->object->object.get());
        if (rel) merkle_root = rel->merkle_root;
    }

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
