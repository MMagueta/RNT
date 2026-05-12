#include "RNT_C_API.h"

#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
#include "InMemoryBackend.h"
#include "SqliteBackend.h"
#include "TupleCodec.h"
#include "VM.h"

#include <picosha2.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Global runtime state (single instance per process)
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
    static std::once_flag           g_init_flag;

    bool is_init() { return g_rt != nullptr; }

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
}

// ---------------------------------------------------------------------------
// Public C API
// ---------------------------------------------------------------------------

int rnt_init(const char* driver, const char* storage_path)
{
    int result = 0;
    std::call_once(g_init_flag, [&]()
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
        }
        catch (...)
        {
            result = -1;
        }
    });
    return result;
}

int rnt_firewall(const char* /*auth_method*/, char** claims_out)
{
    if (!is_init() || !claims_out) return -1;
    // PermissionsManager::Firewall is stubbed; grant all claims for now.
    *claims_out = heap_str("READ WRITE");
    return 0;
}

rnt_handle_t rnt_open_handle(const char* path, const char* /*claims*/)
{
    if (!is_init() || !path) return nullptr;
    return g_rt->handler->Open(split_path(path), nullptr);
}

int rnt_close_handle(rnt_handle_t handle)
{
    if (!is_init() || !handle) return -1;
    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    return g_rt->handler->Close(h) ? 0 : -1;
}

int rnt_branch_payload(rnt_handle_t handle, uint8_t** payload_out, size_t* len_out)
{
    if (!is_init() || !handle || !payload_out || !len_out) return -1;

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
    if (!is_init() || !handle) return -1;

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
    if (!is_init() || !path) return -1;
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
    if (!is_init() || !path) return -1;
    const auto parts = split_path(path);
    if (g_rt->objects.Find(parts) != nullptr) return 0;

    auto branch  = std::make_unique<nt::ObjectManager::Branch>();
    branch->name = parts.empty() ? "" : parts.back();
    if (payload && payload_len > 0)
        branch->payload.assign(payload, payload + payload_len);

    g_rt->objects.Register(parts, std::move(branch), make_branch_type());
    return 0;
}

int rnt_link_tuple(const char* relation_path, const char* kv_attrs, char** hash_out)
{
    if (!is_init() || !relation_path || !kv_attrs || !hash_out) return -1;
    const auto parts = split_path(relation_path);
    auto bytes       = nt::TupleCodec::Serialize(parse_kv(kv_attrs));
    const auto hash  = g_rt->storage->Put(std::move(bytes));
    g_rt->storage->LinkTuple(parts, hash);
    *hash_out = heap_str(hash);
    return 0;
}

int rnt_relation_merkle_root(const char* relation_path, char** root_hash_out)
{
    if (!is_init() || !relation_path || !root_hash_out) return -1;

    const auto parts = split_path(relation_path);

    // Page through all tuple hashes in sorted order (ORDER BY tuple_hash in
    // SqliteBackend) and hash them into a single deterministic root.
    constexpr std::size_t PAGE = 512;
    std::size_t offset = 0;
    std::vector<uint8_t> accumulator;

    for (;;)
    {
        auto page = g_rt->storage->TupleHashes(parts, offset, PAGE);
        if (page.empty()) break;
        for (const auto& h : page)
            accumulator.insert(accumulator.end(), h.begin(), h.end());
        offset += page.size();
        if (page.size() < PAGE) break;
    }

    *root_hash_out = heap_str(
        picosha2::hash256_hex_string(accumulator.begin(), accumulator.end()));
    return 0;
}

rnt_cursor_t rnt_cursor_open(rnt_handle_t handle)
{
    if (!is_init() || !handle) return nullptr;
    auto* h = static_cast<nt::HandlerManager::handle*>(handle);
    return g_rt->cursors->Open(h);
}

int rnt_cursor_next(rnt_cursor_t cursor, char** tuple_out)
{
    if (!is_init() || !cursor || !tuple_out) return -1;
    auto* c = static_cast<nt::CursorManager::cursor*>(cursor);

    nt::PlanNode plan;
    plan.op          = nt::PlanNode::Op::SCAN;
    plan.scan_cursor = c;

    nt::VM vm(*g_rt->cursors);
    nt::Tuple* t = vm.Next(&plan);
    if (!t) { *tuple_out = nullptr; return 0; }
    *tuple_out = heap_str(tuple_to_kv(t));
    return 1;
}

int rnt_cursor_close(rnt_cursor_t cursor)
{
    if (!is_init() || !cursor) return -1;
    g_rt->cursors->Close(static_cast<nt::CursorManager::cursor*>(cursor));
    return 0;
}

void rnt_free_string(char* s)  { delete[] s; }
void rnt_free_bytes(uint8_t* p) { delete[] p; }
