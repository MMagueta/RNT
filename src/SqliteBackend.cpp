#include "SqliteBackend.h"

#include <picosha2.h>
#include <numeric>
#include <stdexcept>

namespace nt
{
    static std::string JoinPath(const std::vector<std::string>& path)
    {
        return std::accumulate(
            std::next(path.begin()), path.end(), path.front(),
            [](std::string a, const std::string& b) { return std::move(a) + "/" + b; });
    }

    SqliteBackend::SqliteBackend(const std::string& path)
    {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db_));

        const char* schema =
            "CREATE TABLE IF NOT EXISTS kv ("
            "  hash  TEXT PRIMARY KEY,"
            "  value BLOB NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS relation_index ("
            "  path       TEXT NOT NULL,"
            "  tuple_hash TEXT NOT NULL,"
            "  UNIQUE(path, tuple_hash)"
            ");";

        char* err = nullptr;
        if (sqlite3_exec(db_, schema, nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::string msg(err);
            sqlite3_free(err);
            throw std::runtime_error(msg);
        }
    }

    SqliteBackend::~SqliteBackend()
    {
        sqlite3_close(db_);
    }

    std::string SqliteBackend::PathKey(const std::vector<std::string>& path)
    {
        return JoinPath(path);
    }

    std::string SqliteBackend::Put(std::vector<uint8_t> value)
    {
        const std::string hash = picosha2::hash256_hex_string(value.begin(), value.end());

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT OR IGNORE INTO kv (hash, value) VALUES (?, ?)",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 2, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return hash;
    }

    std::optional<std::vector<uint8_t>> SqliteBackend::Get(const std::string& hash)
    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT value FROM kv WHERE hash = ?",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<std::vector<uint8_t>> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const auto* blob = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));
            int size = sqlite3_column_bytes(stmt, 0);
            result = std::vector<uint8_t>(blob, blob + size);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    void SqliteBackend::LinkTuple(const std::vector<std::string>& relation_path,
                                  const std::string& tuple_hash)
    {
        const std::string path_key = PathKey(relation_path);

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT OR IGNORE INTO relation_index (path, tuple_hash) VALUES (?, ?)",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, path_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, tuple_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<std::string> SqliteBackend::TupleHashes(
        const std::vector<std::string>& relation_path,
        std::size_t offset,
        std::size_t limit)
    {
        const std::string path_key = PathKey(relation_path);

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT tuple_hash FROM relation_index"
            " WHERE path = ? ORDER BY tuple_hash LIMIT ? OFFSET ?",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, path_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit));
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(offset));

        std::vector<std::string> hashes;
        while (sqlite3_step(stmt) == SQLITE_ROW)
            hashes.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        sqlite3_finalize(stmt);
        return hashes;
    }
}
