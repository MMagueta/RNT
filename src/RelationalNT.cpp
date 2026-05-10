#include "Runtime.h"

#ifdef NT_CONSOLE_APP
#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
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

    // --- Mock: register user in the object registry ---
    nt::ObjectManager objects;

    std::unique_ptr<nt::ObjectManager::object_type> type = std::make_unique<nt::ObjectManager::object_type>();
    type->label = RELATION;
    type->disposable = false;
    type->methods = { OPEN, CLOSE };

    objects.Register({ "multigroups", "sakura", "relations", "user" }, std::make_unique<nt::ObjectManager::Relation>(), std::move(type));

    // --- Mock: populate the cursor store with three tuples ---
    nt::CursorManager cursors;
    cursors.MockInsert({ "multigroups", "sakura", "relations", "user" }, { { "name", "Alice" }, { "age", "30" } });
    cursors.MockInsert({ "multigroups", "sakura", "relations", "user" }, { { "name", "Bob"   }, { "age", "25" } });
    cursors.MockInsert({ "multigroups", "sakura", "relations", "user"}, {{"name", "Carol"}, {"age", "35"}});

    // --- Open a handle on user through the full manager pipeline ---
    nt::PermissionsManager permissions;
    nt::IdentityManager identities;
    nt::LifecycleManager lifecycles;
    nt::HandlerManager handler(objects, permissions, identities, lifecycles);

    int connection = 1;  // dummy connection context
    nt::HandlerManager::handle* handle = handler.Open({ "multigroups", "sakura", "relations", "user" }, &connection);
    if (handle == nullptr)
    {
        std::cout << "Failed to open handle for user\n";
        return 1;
    }

    // --- Open a cursor on the relation ---
    nt::CursorManager::cursor* cursor = cursors.Open(handle);
    if (cursor == nullptr)
    {
        std::cout << "Failed to open cursor for user\n";
        handler.Close(handle);
        return 1;
    }

    // --- Build a SCAN plan and pull all tuples lazily through the FOL VM ---
    nt::PlanNode plan;
    plan.op = nt::PlanNode::Op::SCAN;
    plan.scan_cursor = cursor;

    nt::VM vm(cursors);
    std::cout << "Tuples in user:\n";
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
