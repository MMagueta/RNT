#pragma once

#include "IStorageBackend.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @file Merkle.h
 * @brief Sorted content-addressed B-tree over SHA-256 tuple hash hex strings.
 *
 * Each node is stored as an immutable blob in IStorageBackend via Put/Get.
 * All three operations load only the nodes on the path from root to the target
 * leaf — O(log_B(n)) nodes in memory at once, never the whole tree.
 *
 * Branching factor B = 64: leaves hold up to 64 hashes; internal nodes hold
 * up to 64 child entries.  At B=64 and 1 billion tuples the tree is at most
 * 5 levels deep (~20 KB peak working set).
 *
 * Nodes are immutable once written (content-addressed). Insert and Remove
 * write new nodes only along the modified path; old nodes remain valid for any
 * snapshot that still references them.
 *
 * Internal node entries store (leaf_count, min_hash, child_hash):
 *   - leaf_count enables O(log_B(n)+limit) offset navigation without loading
 *     sibling subtrees.
 *   - min_hash (smallest hash in the subtree) is the routing key.
 *   - child_hash is the content-addressed hash of the child node blob.
 */

namespace nt {

class Merkle {
public:
    static constexpr size_t B = 64;

    /**
     * @brief Insert a tuple hash into the tree.
     * @param store     KV backend for node serialization.
     * @param root_hex  Current root hash (empty string = empty tree).
     * @param hash_hex  64-char hex SHA-256 of the tuple to insert.
     * @return New root hash.  Equal to root_hex when the hash was already present.
     */
    static std::string Insert(IStorageBackend& store,
                               const std::string& root_hex,
                               const std::string& hash_hex);

    /**
     * @brief Remove a tuple hash from the tree.
     * @param store     KV backend.
     * @param root_hex  Current root hash.
     * @param hash_hex  64-char hex SHA-256 of the tuple to remove.
     * @return New root hash.  Empty string when the tree becomes empty.
     */
    static std::string Remove(IStorageBackend& store,
                               const std::string& root_hex,
                               const std::string& hash_hex);

    /**
     * @brief Page through tuple hashes in sorted order.
     *
     * Descends to the leaf at logical position @p offset using cumulative
     * leaf_counts stored in internal nodes, then walks forward collecting at
     * most @p limit hashes.  Only the nodes on the active path are loaded;
     * sibling subtrees are never touched.
     *
     * @param store     KV backend.
     * @param root_hex  Current root hash.
     * @param offset    Zero-based logical tuple offset.
     * @param limit     Maximum number of hashes to return.
     * @return Hex hash strings in sorted order.  Empty when past the end.
     */
    static std::vector<std::string> Page(IStorageBackend& store,
                                          const std::string& root_hex,
                                          size_t offset,
                                          size_t limit);

private:
    using Hash32 = std::array<uint8_t, 32>;

    struct LeafNode {
        std::vector<Hash32> hashes;  // sorted ascending, len ≤ B
    };

    // Wire: 8-byte leaf_count | 32-byte min_hash | 32-byte child_hash
    struct ChildEntry {
        uint64_t leaf_count;  // number of tuple hashes in this subtree
        Hash32   min_hash;    // smallest tuple hash in the subtree
        Hash32   child_hash;  // content-addressed hash of the child node
    };

    struct InternalNode {
        std::vector<ChildEntry> entries;  // sorted by min_hash, len ≤ B
    };

    // Wire format
    static std::vector<uint8_t> encode_leaf(const LeafNode& n);
    static std::vector<uint8_t> encode_internal(const InternalNode& n);
    static LeafNode             decode_leaf(const std::vector<uint8_t>& bytes);
    static InternalNode         decode_internal(const std::vector<uint8_t>& bytes);
    static bool                 is_leaf_bytes(const std::vector<uint8_t>& bytes);

    // Hex / binary
    static Hash32      hex_to_bin(const std::string& hex);
    static std::string bin_to_hex(const Hash32& bin);

    // Storage helpers
    static std::vector<uint8_t> load_node(IStorageBackend& store,
                                           const std::string& hash_hex);
    static std::string store_leaf(IStorageBackend& store, const LeafNode& n);
    static std::string store_internal(IStorageBackend& store, const InternalNode& n);

    // Returns the minimum hash of the subtree rooted at node_hex.
    static Hash32 subtree_min(IStorageBackend& store, const std::string& node_hex);

    // Recursive insert result: new node hash, subtree leaf count, optional split.
    struct InsertResult {
        std::string new_hash;
        uint64_t    leaf_count = 0;
        bool        did_split  = false;
        uint64_t    split_leaf_count = 0;
        Hash32      split_min_hash   = {};
        std::string split_hash;
    };

    static InsertResult insert_into(IStorageBackend& store,
                                     const std::string& node_hex,
                                     const Hash32& tuple_hash);

    // Recursive remove result: new node hash (empty = node gone), leaf count.
    struct RemoveResult {
        std::string new_hash;
        uint64_t    leaf_count = 0;
    };

    static RemoveResult remove_from(IStorageBackend& store,
                                     const std::string& node_hex,
                                     const Hash32& tuple_hash);

    // Page collection — offset consumed as we descend.
    static void page_from(IStorageBackend& store,
                           const std::string& node_hex,
                           size_t& offset,
                           size_t limit,
                           std::vector<Hash32>& out);

    // Find which child to route into (rightmost whose min_hash ≤ target).
    static size_t route(const InternalNode& node, const Hash32& target);
};

} // namespace nt
