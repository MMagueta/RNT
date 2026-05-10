#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * @file IStorageBackend.h
 * @brief Abstract KV storage backend consumed by CursorManager.
 *
 * Storage is strictly content-addressed: every value is stored under its
 * SHA256 hex digest. This mirrors the Sakura physical layer in which the
 * key is always `Sha256.to_hex (Sha256.string serialized_bytes)` and the
 * value is the raw serialized bytes.
 *
 * Relation membership (which tuple hashes belong to a given relation) is
 * tracked via a separate index managed by LinkTuple / TupleHashes. This
 * corresponds to Sakura's B+ tree of tuple hashes stored under each relation.
 */

namespace nt
{
    /**
     * @interface IStorageBackend
     * @brief Contract between CursorManager and a physical (or mock) storage engine.
     *
     * Storage is strictly content-addressed: every value lives under its own
     * SHA256 digest. Relation membership is tracked separately via
     * LinkTuple / TupleHashes, mirroring Sakura's per-relation B+ tree.
     */
    class IStorageBackend
    {
    public:
        virtual ~IStorageBackend() = default;

        /**
         * @brief Stores a serialized value and returns its SHA256 hex hash.
         *
         * Idempotent: storing the same bytes twice does not create a duplicate.
         * The hash is the only valid key for subsequent Get calls.
         */
        virtual std::string Put(std::vector<uint8_t> value) = 0;

        /**
         * @brief Retrieves bytes by their SHA256 hex hash.
         * @return The stored bytes, or std::nullopt when the hash is unknown.
         */
        virtual std::optional<std::vector<uint8_t>> Get(const std::string& hash) = 0;

        /**
         * @brief Records that a tuple (by hash) belongs to a relation (by path).
         *
         * Idempotent. Corresponds to appending a tuple hash to a relation's
         * B+ tree in Sakura's physical layer.
         */
        virtual void LinkTuple(const std::vector<std::string>& relation_path,
                               const std::string& tuple_hash) = 0;

        /**
         * @brief Returns a page of tuple hashes for a relation.
         *
         * Pagination is tuple-level: @p offset and @p limit count whole tuples.
         * Hashes are returned in insertion order.
         *
         * @param relation_path Logical relation path.
         * @param offset        Zero-based index of the first tuple to return.
         * @param limit         Maximum number of tuple hashes to return.
         * @return Ordered list of SHA256 hex hashes. Empty when past the end.
         */
        virtual std::vector<std::string> TupleHashes(
            const std::vector<std::string>& relation_path,
            std::size_t offset,
            std::size_t limit) = 0;
    };
}
