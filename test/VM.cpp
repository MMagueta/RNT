#include <catch2/catch_test_macros.hpp>

#include "CursorManager.h"
#include "HandlerManager.h"
#include "IdentityManager.h"
#include "InMemoryBackend.h"
#include "LifecycleManager.h"
#include "Merkle.h"
#include "NamespaceReferenceManager.h"
#include "ObjectManager.h"
#include "PermissionsManager.h"
#include "TupleCodec.h"
#include "VM.h"

#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Stores a tuple in the backend and updates the Relation object's Merkle root.
static void store_tuple(nt::InMemoryBackend& backend,
                        nt::ObjectManager& objects,
                        const std::vector<std::string>& path,
                        std::vector<nt::Attribute> attrs)
{
    auto bytes = nt::TupleCodec::Serialize(attrs);
    auto hash  = backend.Put(std::move(bytes));

    auto* entry = objects.Find(path);
    if (!entry) return;
    auto* rel = dynamic_cast<nt::ObjectManager::Relation*>(entry->object.get());
    if (!rel) return;
    rel->merkle_root = nt::Merkle::Insert(backend, rel->merkle_root, hash);
}

static std::unique_ptr<nt::ObjectManager::object_type> make_relation_type()
{
    auto t = std::make_unique<nt::ObjectManager::object_type>();
    t->label      = RELATION;
    t->disposable = false;
    t->methods    = { OPEN, CLOSE };
    return t;
}

// Generator for eq[A, B]: returns one tuple when A == B, nothing otherwise.
// Treats both args as integers when parseable, strings otherwise.
static std::unique_ptr<nt::ObjectManager::ephemeral_object_type> make_eq_type()
{
    auto t = std::make_unique<nt::ObjectManager::ephemeral_object_type>();
    t->label       = EPHEMERAL_RELATION;
    t->disposable  = false;
    t->methods     = { OPEN, CLOSE };
    t->cardinality = nt::ObjectManager::ephemeral_object_type::Cardinality::AlephZero;
    t->generator   = [](const std::vector<std::string>& args,
                        std::size_t offset,
                        std::size_t limit) -> std::vector<nt::Tuple>
    {
        if (args.size() < 2 || offset > 0 || limit == 0) return {};
        if (args[0] != args[1]) return {};
        return { nt::Tuple({ { "left", args[0] }, { "right", args[1] } }) };
    };
    return t;
}

// ---------------------------------------------------------------------------
// Fixture: shared manager stack
// ---------------------------------------------------------------------------

struct Fixture
{
    nt::InMemoryBackend  backend;
    nt::ObjectManager    objects;
    nt::PermissionsManager permissions;
    nt::IdentityManager  identities;
    nt::LifecycleManager lifecycles;
    nt::NamespaceReferenceManager references { objects };
    nt::HandlerManager   handler { objects, permissions, identities, lifecycles, references };
    nt::CursorManager    cursors { backend };
    int conn = 1;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("SCAN returns all tuples from a stored relation", "[scan]")
{
    Fixture f;
    const std::vector<std::string> path = { "relations", "villager" };

    f.objects.Register(path, std::make_unique<nt::ObjectManager::Relation>(), make_relation_type());
    store_tuple(f.backend, f.objects, path, { { "name", "John"  }, { "age", "30" } });
    store_tuple(f.backend, f.objects, path, { { "name", "Peter" }, { "age", "25" } });

    auto* handle = f.handler.Open(path, &f.conn);
    REQUIRE(handle != nullptr);
    auto* rel = dynamic_cast<nt::ObjectManager::Relation*>(f.objects.Find(path)->object.get());
    auto* cursor = f.cursors.Open(handle, rel->merkle_root);
    REQUIRE(cursor != nullptr);

    nt::PlanNode plan;
    plan.op          = nt::PlanNode::Op::SCAN;
    plan.scan_cursor = cursor;

    nt::VM vm(f.cursors);
    int count = 0;
    while (vm.Next(&plan)) ++count;

    REQUIRE(count == 2);

    f.cursors.Close(cursor);
    f.handler.Close(handle);
}

TEST_CASE("JOIN filters a stored relation with eq ephemeral", "[join]")
{
    Fixture f;
    const std::vector<std::string> people_path = { "relations", "people" };
    const std::vector<std::string> eq_path     = { "builtins", "eq"     };

    // Register stored relation: people(name, age)
    f.objects.Register(people_path,
                       std::make_unique<nt::ObjectManager::Relation>(),
                       make_relation_type());

    store_tuple(f.backend, f.objects, people_path, { { "name", "John"  }, { "age", "30" } });
    store_tuple(f.backend, f.objects, people_path, { { "name", "Peter" }, { "age", "25" } });
    store_tuple(f.backend, f.objects, people_path, { { "name", "Jude"  }, { "age", "25" } });

    // Register ephemeral eq relation
    f.objects.Register(eq_path,
                       std::make_unique<nt::ObjectManager::Relation>(),
                       make_eq_type());

    // Open handles and cursors
    auto* people_handle = f.handler.Open(people_path, &f.conn);
    REQUIRE(people_handle != nullptr);
    auto* eq_handle = f.handler.Open(eq_path, &f.conn);
    REQUIRE(eq_handle != nullptr);

    auto* people_rel = dynamic_cast<nt::ObjectManager::Relation*>(
        f.objects.Find(people_path)->object.get());
    auto* people_cursor = f.cursors.Open(people_handle, people_rel->merkle_root);
    REQUIRE(people_cursor != nullptr);
    auto* eq_cursor = f.cursors.Open(eq_handle); // EPHEMERAL: starts exhausted, JOIN resets
    REQUIRE(eq_cursor != nullptr);

    // Plan: JOIN( SCAN(people), SCAN(eq[ Var("age"), Const("25") ]) )
    nt::PlanNode right;
    right.op          = nt::PlanNode::Op::SCAN;
    right.scan_cursor = eq_cursor;
    right.scan_args   = { nt::PathArg::Var("age"), nt::PathArg::Const("25") };

    nt::PlanNode left;
    left.op          = nt::PlanNode::Op::SCAN;
    left.scan_cursor = people_cursor;

    nt::PlanNode root;
    root.op    = nt::PlanNode::Op::JOIN;
    root.left  = &left;
    root.right = &right;

    // Execute and collect matched names
    nt::VM vm(f.cursors);
    std::vector<std::string> matched;
    while (nt::Tuple* t = vm.Next(&root))
    {
        for (const auto& attr : t->attrs())
            if (attr.name == "name")
                matched.push_back(attr.value);
    }

    // Jude (age=25) and Peter (age=25) should match; Jogn (age=30) should not
    REQUIRE(matched.size() == 2);
    REQUIRE(std::find(matched.begin(), matched.end(), "Jude")  != matched.end());
    REQUIRE(std::find(matched.begin(), matched.end(), "Peter") != matched.end());
    REQUIRE(std::find(matched.begin(), matched.end(), "John")  == matched.end());

    f.cursors.Close(people_cursor);
    f.cursors.Close(eq_cursor);
    f.handler.Close(people_handle);
    f.handler.Close(eq_handle);
}

TEST_CASE("TAKE limits tuples emitted from a SCAN", "[take]")
{
    Fixture f;
    const std::vector<std::string> path = { "relations", "items" };

    f.objects.Register(path, std::make_unique<nt::ObjectManager::Relation>(), make_relation_type());
    store_tuple(f.backend, f.objects, path, { { "id", "1" } });
    store_tuple(f.backend, f.objects, path, { { "id", "2" } });
    store_tuple(f.backend, f.objects, path, { { "id", "3" } });

    auto* handle = f.handler.Open(path, &f.conn);
    REQUIRE(handle != nullptr);
    auto* rel = dynamic_cast<nt::ObjectManager::Relation*>(f.objects.Find(path)->object.get());
    auto* cursor = f.cursors.Open(handle, rel->merkle_root);
    REQUIRE(cursor != nullptr);

    nt::PlanNode scan;
    scan.op          = nt::PlanNode::Op::SCAN;
    scan.scan_cursor = cursor;

    nt::PlanNode take;
    take.op         = nt::PlanNode::Op::TAKE;
    take.left       = &scan;
    take.take_limit = 2;

    nt::VM vm(f.cursors);
    int count = 0;
    while (vm.Next(&take)) ++count;

    REQUIRE(count == 2);

    f.cursors.Close(cursor);
    f.handler.Close(handle);
}
