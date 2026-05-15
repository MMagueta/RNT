#pragma once

#include "IStorageBackend.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * @file MultigroupCodec.h
 * @brief Content-addressing utilities for multigroup snapshots.
 *
 * A multigroup snapshot is identified by the SHA256 of the serialized list of
 * its child relations: pairs of (relation_name, relation_merkle_root). The
 * pairs are sorted by relation_name before encoding so the hash is
 * insertion-order independent.
 *
 * The same backend that stores tuple bytes and Merkle nodes also stores the
 * multigroup snapshot blob — Put returns the multigroup hash. This is what
 * makes "checkout an arbitrary commit hash" well-defined from cold storage:
 * given any multigroup hash, Get returns the serialized child list, from
 * which the runtime can reconstruct the snapshot in memory.
 *
 * Both stored RELATIONs and EPHEMERAL_RELATIONs are represented uniformly
 * here — only the relation's name and its current merkle_root participate
 * in the multigroup hash, regardless of how that root was derived.
 */

namespace nt::MultigroupCodec
{
    /**
     * @brief One entry in a multigroup snapshot: (relation_name, merkle_root).
     *
     * The merkle_root is whatever the relation reports as its current root —
     * a tuple Merkle root for stored relations, or a generator/schema/deps
     * hash for ephemeral relations. The codec does not distinguish between
     * the two; it just hashes the pair list.
     */
    using RelationEntry = std::pair<std::string, std::string>;

    /**
     * @brief Computes the SHA256 hex digest of the serialized relation list.
     *
     * Equivalent to SHA256(Serialize(relations)), so the value returned here
     * matches the key returned by IStorageBackend::Put(Serialize(relations)).
     *
     * @param relations Child relations as (name, merkle_root) pairs. Need not
     *                  be pre-sorted.
     * @return 64-character lowercase hex string.
     */
    std::string Hash(const std::vector<RelationEntry>& relations);

    /**
     * @brief Serializes the relation list to raw bytes for KV storage.
     *
     * Pairs are sorted by relation_name before encoding so the bytes (and
     * thus the hash) are insertion-order independent.
     *
     * Wire format: a sequence of length-prefixed UTF-8 pairs.
     *   [uint32_le name_len][name bytes][uint32_le root_len][root bytes] ...
     *
     * @param relations Child relations as (name, merkle_root) pairs.
     * @return Serialized bytes.
     */
    std::vector<uint8_t> Serialize(const std::vector<RelationEntry>& relations);

    /**
     * @brief Reconstructs the relation list from bytes returned by Serialize().
     * @return Relations in serialization (i.e. sorted) order.
     */
    std::vector<RelationEntry> Deserialize(const std::vector<uint8_t>& bytes);

    /**
     * @brief Serializes the relation list, stores it via the backend, and
     *        returns the resulting multigroup hash.
     *
     * Convenience wrapper around Serialize() + IStorageBackend::Put.  Use this
     * when committing a snapshot so the multigroup blob is persisted in the
     * same content-addressed store as tuple Merkle nodes.
     *
     * @param store     Storage backend.
     * @param relations Child relations as (name, merkle_root) pairs.
     * @return Multigroup hash (64-char lowercase hex).
     */
    std::string Store(IStorageBackend& store,
                      const std::vector<RelationEntry>& relations);
}
