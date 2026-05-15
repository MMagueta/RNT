#include <catch2/catch_test_macros.hpp>

#include "InMemoryBackend.h"
#include "MultigroupCodec.h"

#include <vector>

// ---------------------------------------------------------------------------
// Step 1 — Multigroup snapshots first-class
//
// MultigroupCodec is what makes a snapshot identifier well-defined: it
// hashes the list of (relation_name, relation_merkle_root) pairs after
// sorting them by name, so two snapshots with the same content collapse
// to the same hash regardless of insertion order. The same blob is stored
// in the content-addressed backend, which is what makes "open a snapshot
// by hash" well-defined even after the in-memory registry forgets it.
// ---------------------------------------------------------------------------

TEST_CASE("Hash is order-independent — sort by relation name", "[step1][multigroup-codec]")
{
    std::vector<nt::MultigroupCodec::RelationEntry> in_order = {
        {"alpha",   "AAA"},
        {"bravo",   "BBB"},
        {"charlie", "CCC"},
    };
    std::vector<nt::MultigroupCodec::RelationEntry> shuffled = {
        {"charlie", "CCC"},
        {"alpha",   "AAA"},
        {"bravo",   "BBB"},
    };
    REQUIRE(nt::MultigroupCodec::Hash(in_order) == nt::MultigroupCodec::Hash(shuffled));
}

TEST_CASE("Hash distinguishes content changes", "[step1][multigroup-codec]")
{
    std::vector<nt::MultigroupCodec::RelationEntry> base = {{"foo", "AAA"}};

    std::vector<nt::MultigroupCodec::RelationEntry> different_root = {{"foo", "BBB"}};
    std::vector<nt::MultigroupCodec::RelationEntry> different_name = {{"bar", "AAA"}};
    std::vector<nt::MultigroupCodec::RelationEntry> extra_member   = {{"foo", "AAA"}, {"bar", "BBB"}};

    REQUIRE(nt::MultigroupCodec::Hash(base) != nt::MultigroupCodec::Hash(different_root));
    REQUIRE(nt::MultigroupCodec::Hash(base) != nt::MultigroupCodec::Hash(different_name));
    REQUIRE(nt::MultigroupCodec::Hash(base) != nt::MultigroupCodec::Hash(extra_member));
}

TEST_CASE("Serialize/Deserialize round-trip preserves entries in sorted order",
          "[step1][multigroup-codec]")
{
    std::vector<nt::MultigroupCodec::RelationEntry> entries = {
        {"foo", "0011"},
        {"bar", "2233"},
        {"baz", ""},
    };
    auto bytes   = nt::MultigroupCodec::Serialize(entries);
    auto decoded = nt::MultigroupCodec::Deserialize(bytes);

    REQUIRE(decoded.size() == 3);
    REQUIRE(decoded[0].first  == "bar");
    REQUIRE(decoded[0].second == "2233");
    REQUIRE(decoded[1].first  == "baz");
    REQUIRE(decoded[1].second == "");
    REQUIRE(decoded[2].first  == "foo");
    REQUIRE(decoded[2].second == "0011");
}

TEST_CASE("Store puts the snapshot blob and returns a hash matching Hash()",
          "[step1][multigroup-codec]")
{
    nt::InMemoryBackend backend;
    std::vector<nt::MultigroupCodec::RelationEntry> entries = {
        {"foo", "abc"},
        {"bar", "def"},
    };

    const std::string stored   = nt::MultigroupCodec::Store(backend, entries);
    const std::string computed = nt::MultigroupCodec::Hash(entries);
    REQUIRE(stored == computed);

    // Cold-load: bytes retrievable by hash, decode back to the same entries.
    auto bytes = backend.Get(stored);
    REQUIRE(bytes.has_value());
    auto decoded = nt::MultigroupCodec::Deserialize(*bytes);
    REQUIRE(decoded.size() == 2);
    REQUIRE(decoded[0].first == "bar");
    REQUIRE(decoded[1].first == "foo");
}

TEST_CASE("Empty relation list hashes deterministically", "[step1][multigroup-codec]")
{
    std::vector<nt::MultigroupCodec::RelationEntry> empty;
    const std::string h1 = nt::MultigroupCodec::Hash(empty);
    const std::string h2 = nt::MultigroupCodec::Hash(empty);
    REQUIRE(h1 == h2);
    REQUIRE(h1.size() == 64);  // SHA256 hex
}
