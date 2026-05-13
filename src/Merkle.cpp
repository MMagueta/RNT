#include "Merkle.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace nt {

// ── Wire format constants ────────────────────────────────────────────────────

static constexpr uint8_t TAG_LEAF     = 0x4C; // 'L'
static constexpr uint8_t TAG_INTERNAL = 0x49; // 'I'

// ── Byte-order helpers ───────────────────────────────────────────────────────

static void write_u32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(static_cast<uint8_t>(v >> 24));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >>  8));
    buf.push_back(static_cast<uint8_t>(v      ));
}

static void write_u64(std::vector<uint8_t>& buf, uint64_t v)
{
    buf.push_back(static_cast<uint8_t>(v >> 56));
    buf.push_back(static_cast<uint8_t>(v >> 48));
    buf.push_back(static_cast<uint8_t>(v >> 40));
    buf.push_back(static_cast<uint8_t>(v >> 32));
    buf.push_back(static_cast<uint8_t>(v >> 24));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >>  8));
    buf.push_back(static_cast<uint8_t>(v      ));
}

static uint32_t read_u32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
}

static uint64_t read_u64(const uint8_t* p)
{
    return (uint64_t(p[0]) << 56) | (uint64_t(p[1]) << 48)
         | (uint64_t(p[2]) << 40) | (uint64_t(p[3]) << 32)
         | (uint64_t(p[4]) << 24) | (uint64_t(p[5]) << 16)
         | (uint64_t(p[6]) <<  8) |  uint64_t(p[7]);
}

// ── Hex / binary conversion ──────────────────────────────────────────────────

Merkle::Hash32 Merkle::hex_to_bin(const std::string& hex)
{
    if (hex.size() != 64)
        throw std::invalid_argument(
            "Merkle: hash must be exactly 64 hex characters, got " +
            std::to_string(hex.size()));

    Hash32 out{};
    auto nibble = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        throw std::invalid_argument(std::string("Merkle: invalid hex character '") + c + "'");
    };
    for (size_t i = 0; i < 32; ++i)
        out[i] = static_cast<uint8_t>((nibble(hex[2*i]) << 4) | nibble(hex[2*i+1]));
    return out;
}

std::string Merkle::bin_to_hex(const Hash32& bin)
{
    static const char HEX[] = "0123456789abcdef";
    std::string out(64, '0');
    for (size_t i = 0; i < 32; ++i)
    {
        out[2*i]   = HEX[(bin[i] >> 4) & 0xF];
        out[2*i+1] = HEX[bin[i] & 0xF];
    }
    return out;
}

// ── Node serialisation ───────────────────────────────────────────────────────

std::vector<uint8_t> Merkle::encode_leaf(const LeafNode& n)
{
    std::vector<uint8_t> buf;
    buf.reserve(1 + 4 + n.hashes.size() * 32);
    buf.push_back(TAG_LEAF);
    write_u32(buf, static_cast<uint32_t>(n.hashes.size()));
    for (const auto& h : n.hashes)
        buf.insert(buf.end(), h.begin(), h.end());
    return buf;
}

std::vector<uint8_t> Merkle::encode_internal(const InternalNode& n)
{
    // Each entry: 8 (leaf_count) + 32 (min_hash) + 32 (child_hash) = 72 bytes
    std::vector<uint8_t> buf;
    buf.reserve(1 + 4 + n.entries.size() * 72);
    buf.push_back(TAG_INTERNAL);
    write_u32(buf, static_cast<uint32_t>(n.entries.size()));
    for (const auto& e : n.entries)
    {
        write_u64(buf, e.leaf_count);
        buf.insert(buf.end(), e.min_hash.begin(), e.min_hash.end());
        buf.insert(buf.end(), e.child_hash.begin(), e.child_hash.end());
    }
    return buf;
}

Merkle::LeafNode Merkle::decode_leaf(const std::vector<uint8_t>& bytes)
{
    uint32_t count = read_u32(bytes.data() + 1);
    LeafNode n;
    n.hashes.reserve(count);
    const uint8_t* p = bytes.data() + 5;
    for (uint32_t i = 0; i < count; ++i, p += 32)
    {
        Hash32 h;
        std::memcpy(h.data(), p, 32);
        n.hashes.push_back(h);
    }
    return n;
}

Merkle::InternalNode Merkle::decode_internal(const std::vector<uint8_t>& bytes)
{
    uint32_t count = read_u32(bytes.data() + 1);
    InternalNode n;
    n.entries.reserve(count);
    const uint8_t* p = bytes.data() + 5;
    for (uint32_t i = 0; i < count; ++i, p += 72)
    {
        ChildEntry e;
        e.leaf_count = read_u64(p);
        std::memcpy(e.min_hash.data(),   p + 8,  32);
        std::memcpy(e.child_hash.data(), p + 40, 32);
        n.entries.push_back(e);
    }
    return n;
}

bool Merkle::is_leaf_bytes(const std::vector<uint8_t>& bytes)
{
    return !bytes.empty() && bytes[0] == TAG_LEAF;
}

// ── Storage helpers ──────────────────────────────────────────────────────────

std::vector<uint8_t> Merkle::load_node(IStorageBackend& store,
                                        const std::string& hash_hex)
{
    auto opt = store.Get(hash_hex);
    if (!opt)
        throw std::runtime_error("Merkle: node not found: " + hash_hex);
    return std::move(*opt);
}

std::string Merkle::store_leaf(IStorageBackend& store, const LeafNode& n)
{
    return store.Put(encode_leaf(n));
}

std::string Merkle::store_internal(IStorageBackend& store, const InternalNode& n)
{
    return store.Put(encode_internal(n));
}

Merkle::Hash32 Merkle::subtree_min(IStorageBackend& store, const std::string& node_hex)
{
    auto bytes = load_node(store, node_hex);
    if (is_leaf_bytes(bytes))
    {
        auto leaf = decode_leaf(bytes);
        assert(!leaf.hashes.empty() && "Merkle invariant: leaf node must never be empty");
        return leaf.hashes.front();
    }
    auto node = decode_internal(bytes);
    assert(!node.entries.empty() && "Merkle invariant: internal node must never be empty");
    return node.entries.front().min_hash;
}

// ── Routing helper ───────────────────────────────────────────────────────────

size_t Merkle::route(const InternalNode& node, const Hash32& target)
{
    assert(!node.entries.empty() && "Merkle invariant: internal node must never be empty");
    assert(std::is_sorted(node.entries.begin(), node.entries.end(),
               [](const ChildEntry& a, const ChildEntry& b) {
                   return a.min_hash < b.min_hash;
               }) && "Merkle invariant: internal node entries must be sorted by min_hash");

    // Rightmost child whose min_hash ≤ target.
    // Defaults to 0 (leftmost) when target < all min_hashes.
    size_t idx = 0;
    for (size_t i = 1; i < node.entries.size(); ++i)
    {
        if (node.entries[i].min_hash <= target)
            idx = i;
        else
            break;
    }
    return idx;
}

// ── Insert ───────────────────────────────────────────────────────────────────

std::string Merkle::Insert(IStorageBackend& store,
                             const std::string& root_hex,
                             const std::string& hash_hex)
{
    const Hash32 tuple_hash = hex_to_bin(hash_hex);

    if (root_hex.empty())
    {
        LeafNode leaf;
        leaf.hashes.push_back(tuple_hash);
        return store_leaf(store, leaf);
    }

    auto result = insert_into(store, root_hex, tuple_hash);
    if (!result.did_split)
        return result.new_hash;

    // Root split: wrap both halves in a new internal root.
    Hash32 left_min  = subtree_min(store, result.new_hash);
    Hash32 right_min = result.split_min_hash;

    InternalNode new_root;

    ChildEntry left;
    left.leaf_count = result.leaf_count;
    left.min_hash   = left_min;
    left.child_hash = hex_to_bin(result.new_hash);
    new_root.entries.push_back(left);

    ChildEntry right;
    right.leaf_count = result.split_leaf_count;
    right.min_hash   = right_min;
    right.child_hash = hex_to_bin(result.split_hash);
    new_root.entries.push_back(right);

    return store_internal(store, new_root);
}

Merkle::InsertResult Merkle::insert_into(IStorageBackend& store,
                                           const std::string& node_hex,
                                           const Hash32& tuple_hash)
{
    auto bytes = load_node(store, node_hex);

    // ── Leaf node ───────────────────────────────────────────────────────────
    if (is_leaf_bytes(bytes))
    {
        auto leaf = decode_leaf(bytes);

        auto pos = std::lower_bound(leaf.hashes.begin(), leaf.hashes.end(), tuple_hash);
        if (pos != leaf.hashes.end() && *pos == tuple_hash)
        {
            // Already present — no-op.
            InsertResult r;
            r.new_hash   = node_hex;
            r.leaf_count = leaf.hashes.size();
            return r;
        }

        leaf.hashes.insert(pos, tuple_hash);

        if (leaf.hashes.size() <= B)
        {
            InsertResult r;
            r.new_hash   = store_leaf(store, leaf);
            r.leaf_count = leaf.hashes.size();
            return r;
        }

        // Split leaf at midpoint.
        size_t mid = leaf.hashes.size() / 2;
        LeafNode left_leaf, right_leaf;
        left_leaf.hashes  = { leaf.hashes.begin(), leaf.hashes.begin() + static_cast<ptrdiff_t>(mid) };
        right_leaf.hashes = { leaf.hashes.begin() + static_cast<ptrdiff_t>(mid), leaf.hashes.end() };

        InsertResult r;
        r.new_hash         = store_leaf(store, left_leaf);
        r.leaf_count       = left_leaf.hashes.size();
        r.did_split        = true;
        r.split_leaf_count = right_leaf.hashes.size();
        r.split_min_hash   = right_leaf.hashes.front();
        r.split_hash       = store_leaf(store, right_leaf);
        return r;
    }

    // ── Internal node ───────────────────────────────────────────────────────
    auto node = decode_internal(bytes);

    size_t child_idx = route(node, tuple_hash);
    auto child_result = insert_into(store,
                                     bin_to_hex(node.entries[child_idx].child_hash),
                                     tuple_hash);

    node.entries[child_idx].leaf_count = child_result.leaf_count;
    node.entries[child_idx].child_hash = hex_to_bin(child_result.new_hash);

    // Update min_hash for the modified child — only child 0 can have its min
    // decrease (routing guarantees min_hash[i>0] ≤ inserted hash, so inserting
    // below min_hash[i>0] is impossible).
    if (child_idx == 0)
        node.entries[0].min_hash = subtree_min(store, child_result.new_hash);

    uint64_t total = 0;
    for (const auto& e : node.entries) total += e.leaf_count;

    if (!child_result.did_split)
    {
        InsertResult r;
        r.new_hash   = store_internal(store, node);
        r.leaf_count = total;
        return r;
    }

    // Insert the right sibling immediately after child_idx.
    ChildEntry sibling;
    sibling.leaf_count = child_result.split_leaf_count;
    sibling.min_hash   = child_result.split_min_hash;
    sibling.child_hash = hex_to_bin(child_result.split_hash);
    node.entries.insert(node.entries.begin() + static_cast<ptrdiff_t>(child_idx) + 1, sibling);
    total += child_result.split_leaf_count;

    if (node.entries.size() <= B)
    {
        InsertResult r;
        r.new_hash   = store_internal(store, node);
        r.leaf_count = total;
        return r;
    }

    // Split this internal node.
    size_t mid = node.entries.size() / 2;
    InternalNode left_node, right_node;
    left_node.entries  = { node.entries.begin(), node.entries.begin() + static_cast<ptrdiff_t>(mid) };
    right_node.entries = { node.entries.begin() + static_cast<ptrdiff_t>(mid), node.entries.end() };

    uint64_t left_count = 0, right_count = 0;
    for (const auto& e : left_node.entries)  left_count  += e.leaf_count;
    for (const auto& e : right_node.entries) right_count += e.leaf_count;

    InsertResult r;
    r.new_hash         = store_internal(store, left_node);
    r.leaf_count       = left_count;
    r.did_split        = true;
    r.split_leaf_count = right_count;
    r.split_min_hash   = right_node.entries.front().min_hash;
    r.split_hash       = store_internal(store, right_node);
    return r;
}

// ── Remove ───────────────────────────────────────────────────────────────────

std::string Merkle::Remove(IStorageBackend& store,
                             const std::string& root_hex,
                             const std::string& hash_hex)
{
    if (root_hex.empty()) return "";
    const Hash32 tuple_hash = hex_to_bin(hash_hex);
    auto result = remove_from(store, root_hex, tuple_hash);
    if (result.new_hash.empty()) return "";

    // Collapse: if the root is an internal node with a single child, skip it.
    auto bytes = load_node(store, result.new_hash);
    if (!is_leaf_bytes(bytes))
    {
        auto node = decode_internal(bytes);
        if (node.entries.size() == 1)
            return bin_to_hex(node.entries[0].child_hash);
    }
    return result.new_hash;
}

Merkle::RemoveResult Merkle::remove_from(IStorageBackend& store,
                                           const std::string& node_hex,
                                           const Hash32& tuple_hash)
{
    auto bytes = load_node(store, node_hex);

    // ── Leaf node ───────────────────────────────────────────────────────────
    if (is_leaf_bytes(bytes))
    {
        auto leaf = decode_leaf(bytes);
        auto it = std::lower_bound(leaf.hashes.begin(), leaf.hashes.end(), tuple_hash);
        if (it == leaf.hashes.end() || *it != tuple_hash)
            return { node_hex, leaf.hashes.size() };  // not found — no change

        leaf.hashes.erase(it);
        if (leaf.hashes.empty())
            return { "", 0 };
        return { store_leaf(store, leaf), leaf.hashes.size() };
    }

    // ── Internal node ───────────────────────────────────────────────────────
    auto node = decode_internal(bytes);

    size_t child_idx = route(node, tuple_hash);
    auto child_result = remove_from(store,
                                     bin_to_hex(node.entries[child_idx].child_hash),
                                     tuple_hash);

    if (child_result.new_hash.empty())
    {
        node.entries.erase(node.entries.begin() + static_cast<ptrdiff_t>(child_idx));
        if (node.entries.empty()) return { "", 0 };
    }
    else
    {
        node.entries[child_idx].leaf_count = child_result.leaf_count;
        node.entries[child_idx].child_hash = hex_to_bin(child_result.new_hash);
        // The min of any child might have changed if we removed its minimum entry.
        node.entries[child_idx].min_hash   = subtree_min(store, child_result.new_hash);
    }

    uint64_t total = 0;
    for (const auto& e : node.entries) total += e.leaf_count;
    return { store_internal(store, node), total };
}

// ── Page ─────────────────────────────────────────────────────────────────────

std::vector<std::string> Merkle::Page(IStorageBackend& store,
                                       const std::string& root_hex,
                                       size_t offset,
                                       size_t limit)
{
    if (root_hex.empty() || limit == 0) return {};
    std::vector<Hash32> raw;
    raw.reserve(limit);
    size_t skip = offset;
    page_from(store, root_hex, skip, limit, raw);
    std::vector<std::string> out;
    out.reserve(raw.size());
    for (const auto& h : raw) out.push_back(bin_to_hex(h));
    return out;
}

void Merkle::page_from(IStorageBackend& store,
                        const std::string& node_hex,
                        size_t& offset,
                        size_t limit,
                        std::vector<Hash32>& out)
{
    auto bytes = load_node(store, node_hex);

    if (is_leaf_bytes(bytes))
    {
        auto leaf = decode_leaf(bytes);
        const size_t available = leaf.hashes.size();

        if (offset >= available)
        {
            offset -= available;
            return;
        }

        for (size_t i = offset; i < available && out.size() < limit; ++i)
            out.push_back(leaf.hashes[i]);

        offset = 0;
        return;
    }

    auto node = decode_internal(bytes);

    for (const auto& entry : node.entries)
    {
        if (out.size() >= limit) break;

        if (offset >= entry.leaf_count)
        {
            offset -= entry.leaf_count;
            continue;
        }

        page_from(store, bin_to_hex(entry.child_hash), offset, limit, out);
    }
}

} // namespace nt
