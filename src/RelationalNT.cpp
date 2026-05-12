#include "Runtime.h"

#ifdef NT_CONSOLE_APP
#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "Merkle.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
#include "SqliteBackend.h"
#include "TupleCodec.h"
#include "VM.h"

#include <iostream>
#endif

namespace nt
{
    bool NT::IsRunning() const
    {
        return true;
    }

    void NT::SimulateEntryCall()
    {
    }
}

#ifdef NT_CONSOLE_APP
int main()
{
    const nt::NT runtime;
    std::cout << "RNT running: " << (runtime.IsRunning() ? "yes" : "no") << '\n';

    // --- Mock: register villager in the object registry ---
    nt::ObjectManager objects;

    std::unique_ptr<nt::ObjectManager::object_type> type = std::make_unique<nt::ObjectManager::object_type>();
    type->label = RELATION;
    type->disposable = false;
    type->methods = { OPEN, CLOSE };

    const std::vector<std::string> path = { "multigroups", "sakura", "relations", "villager" };
    auto* villager_rel = new nt::ObjectManager::Relation();
    objects.Register(path, std::unique_ptr<nt::ObjectManager::Relation>(villager_rel), std::move(type));

    // --- Populate backend with three tuples via the Merkle tree ---
    nt::SqliteBackend backend;
    auto store = [&](std::vector<nt::Attribute> attrs) {
        auto bytes = nt::TupleCodec::Serialize(attrs);
        auto hash  = backend.Put(std::move(bytes));
        villager_rel->merkle_root =
            nt::Merkle::Insert(backend, villager_rel->merkle_root, hash);
    };
    store({ { "name", "Blathers" }, { "profession", "Museum Curator" } });
    store({ { "name", "Rover"   }, { "profession", "Traveller" } });
    store({ { "name", "K.K." }, { "profession", "Artist" } });
    nt::CursorManager cursors(backend);

    // --- Open a handle on villager through the full manager pipeline ---
    nt::PermissionsManager permissions;
    nt::IdentityManager identities;
    nt::LifecycleManager lifecycles;
    nt::HandlerManager handler(objects, permissions, identities, lifecycles);

    int connection = 1;  // dummy connection context
    nt::HandlerManager::handle* handle = handler.Open(path, &connection);
    if (handle == nullptr)
    {
        std::cout << "Failed to open handle for villager\n";
        return 1;
    }

    // --- Open a cursor on the relation ---
    nt::CursorManager::cursor* cursor = cursors.Open(handle, villager_rel->merkle_root);
    if (cursor == nullptr)
    {
        std::cout << "Failed to open cursor for villager\n";
        handler.Close(handle);
        return 1;
    }

    // --- Build a SCAN plan and pull all tuples lazily through the FOL VM ---
    nt::PlanNode plan;
    plan.op = nt::PlanNode::Op::SCAN;
    plan.scan_cursor = cursor;

    nt::VM vm(cursors);
    std::cout << "Tuples in villager:\n";
    while (nt::Tuple* t = vm.Next(&plan))
    {
        std::cout << "  ";
        const nt::Attribute* attr = t->Next();
        while (attr != nullptr)
        {
            std::cout << attr->name << "=" << attr->value;
            attr = t->Next();
            if (attr != nullptr)
                std::cout << ", ";
        }
        std::cout << '\n';
    }

    // --- Cleanup ---
    cursors.Close(cursor);
    handler.Close(handle);

#ifdef NT_WAIT_ON_EXIT
    std::cout << "Press Enter to exit...";
    std::cin.get();
#endif

    return runtime.IsRunning() ? 0 : 1;
}
#endif
