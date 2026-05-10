#pragma once

#include "Api.h"
#include "IStorageBackend.h"

#include <sqlite3.h>
#include <string>
#include <vector>

/**
 * @file SqliteBackend.h
 * @brief IStorageBackend implementation backed by an in-process SQLite database.
 *
 * SQLite schema:
 *
 *   kv(hash TEXT PRIMARY KEY, value BLOB NOT NULL)
 *     — content-addressed store; key is the SHA256 hex digest of value.
 *
 *   relation_index(path TEXT NOT NULL, tuple_hash TEXT NOT NULL,
 *                  UNIQUE(path, tuple_hash))
 *     — maps a relation's logical path to the ordered set of tuple hashes
 *       it contains. Corresponds to Sakura's per-relation B+ tree of
 *       tuple hashes stored in the physical layer.
 */

namespace nt
{
    /**
     * @class SqliteBackend
     * @brief IStorageBackend that persists data in a SQLite database.
     *
     * Constructed with a file path or ":memory:" for an in-process database.
     * All KV values are opaque blobs; callers are responsible for serialization
     * (see TupleCodec).
     */
    class NT_API SqliteBackend : public IStorageBackend
    {
    public:
        /**
         * @brief Opens (or creates) a SQLite database at @p path.
         * @param path File path, or ":memory:" for an in-process database.
         * @throws std::runtime_error if the database cannot be opened.
         */
        explicit SqliteBackend(const std::string& path = ":memory:");
        ~SqliteBackend() override;

        std::string Put(std::vector<uint8_t> value) override;
        std::optional<std::vector<uint8_t>> Get(const std::string& hash) override;
        void LinkTuple(const std::vector<std::string>& relation_path,
                       const std::string& tuple_hash) override;
        std::vector<std::string> TupleHashes(const std::vector<std::string>& relation_path,
                                             std::size_t offset,
                                             std::size_t limit) override;

    private:
        sqlite3* db_ = nullptr;

        static std::string PathKey(const std::vector<std::string>& path);
    };
}
