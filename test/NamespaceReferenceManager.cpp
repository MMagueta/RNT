#include <catch2/catch_test_macros.hpp>

#include "NamespaceReferenceManager.h"
#include "ObjectManager.h"

#include <memory>

// ---------------------------------------------------------------------------
// Step 3+4 — Resolver
//
// NamespaceReferenceManager::Resolve is the reparse layer. It rewrites a
// branch-relative path into a snapshot-relative path by reading the named
// Branch object. Step 5 extends it with session overrides: paths beginning
// with /system/sessions/<X>/branches/<n>/... consult the session's
// override map first and fall back to the global branch HEAD when no
// override is present.
// ---------------------------------------------------------------------------

namespace
{
    using Path = std::vector<std::string>;

    std::unique_ptr<nt::ObjectManager::object_type> make_type(OBJECT_TYPE label)
    {
        auto t      = std::make_unique<nt::ObjectManager::object_type>();
        t->label    = label;
        t->methods  = { OPEN, CLOSE };
        return t;
    }

    void register_snapshot(nt::ObjectManager& om, const std::string& hash)
    {
        auto mg = std::make_unique<nt::ObjectManager::Multigroup>();
        mg->merkle_root = hash;
        om.Register({"system", "snapshots", hash}, std::move(mg), make_type(MULTIGROUP));
    }

    void register_branch(nt::ObjectManager& om,
                         const std::string& name,
                         const std::string& target_hash)
    {
        auto br         = std::make_unique<nt::ObjectManager::Branch>();
        br->name        = name;
        br->target_hash = target_hash;
        om.Register({"system", "branches", name}, std::move(br), make_type(BRANCH));
    }

    void register_session(nt::ObjectManager& om,
                          const std::string& id,
                          std::map<std::string, std::string> overrides = {})
    {
        auto s = std::make_unique<nt::ObjectManager::Session>();
        s->branch_overrides = std::move(overrides);
        om.Register({"system", "sessions", id}, std::move(s), make_type(SESSION));
    }
}

TEST_CASE("Branch sub-path rewrites to snapshot sub-path", "[step4][resolve]")
{
    nt::ObjectManager om;
    register_snapshot(om, "snap_main");
    register_branch(om, "main", "snap_main");

    nt::NamespaceReferenceManager nrm(om);
    REQUIRE(nrm.Resolve({"system", "branches", "main", "relations", "items"})
            == Path{"system", "snapshots", "snap_main", "relations", "items"});
}

TEST_CASE("Resolve on the branch object itself is left unchanged", "[step4][resolve]")
{
    nt::ObjectManager om;
    register_snapshot(om, "snap_main");
    register_branch(om, "main", "snap_main");

    nt::NamespaceReferenceManager nrm(om);
    const Path p{"system", "branches", "main"};
    REQUIRE(nrm.Resolve(p) == p);
}

TEST_CASE("Resolve on an unborn branch leaves the path unchanged",
          "[step4][resolve]")
{
    nt::ObjectManager om;
    register_branch(om, "main", "");  // unborn

    nt::NamespaceReferenceManager nrm(om);
    const Path p{"system", "branches", "main", "relations", "items"};
    // No rewrite happens; caller will fail Find on this path, which is the
    // intended behaviour (you can't read relations from an unborn branch).
    REQUIRE(nrm.Resolve(p) == p);
}

TEST_CASE("Resolve on an unknown branch leaves the path unchanged",
          "[step4][resolve]")
{
    nt::ObjectManager om;
    nt::NamespaceReferenceManager nrm(om);
    const Path p{"system", "branches", "ghost", "relations", "items"};
    REQUIRE(nrm.Resolve(p) == p);
}

TEST_CASE("Non-branch paths pass through unchanged", "[step4][resolve]")
{
    nt::ObjectManager om;
    nt::NamespaceReferenceManager nrm(om);
    const Path p{"relations", "anywhere"};
    REQUIRE(nrm.Resolve(p) == p);
}

TEST_CASE("Session override takes precedence over the global branch HEAD",
          "[step5][resolve]")
{
    nt::ObjectManager om;
    register_snapshot(om, "snap_global");
    register_snapshot(om, "snap_session");
    register_branch(om, "main", "snap_global");
    register_session(om, "sess123", {{"main", "snap_session"}});

    nt::NamespaceReferenceManager nrm(om);
    REQUIRE(nrm.Resolve({"system", "sessions", "sess123", "branches", "main",
                         "relations", "items"})
            == Path{"system", "snapshots", "snap_session", "relations", "items"});
}

TEST_CASE("Session resolution falls back to global branch when override is absent",
          "[step5][resolve]")
{
    nt::ObjectManager om;
    register_snapshot(om, "snap_global");
    register_branch(om, "main", "snap_global");
    register_session(om, "sess123");  // no overrides

    nt::NamespaceReferenceManager nrm(om);
    REQUIRE(nrm.Resolve({"system", "sessions", "sess123", "branches", "main",
                         "relations", "items"})
            == Path{"system", "snapshots", "snap_global", "relations", "items"});
}

TEST_CASE("Session resolution with neither override nor global branch is unchanged",
          "[step5][resolve]")
{
    nt::ObjectManager om;
    register_session(om, "sess123");

    nt::NamespaceReferenceManager nrm(om);
    const Path p{"system", "sessions", "sess123", "branches", "main",
                 "relations", "items"};
    REQUIRE(nrm.Resolve(p) == p);
}

TEST_CASE("Empty input path is returned unchanged", "[step4][resolve]")
{
    nt::ObjectManager om;
    nt::NamespaceReferenceManager nrm(om);
    REQUIRE(nrm.Resolve({}).empty());
}
