#pragma once

#include "Api.h"
#include "IStorageBackend.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file InMemoryBackend.h
 * @brief In-process KV storage backend backed by unordered_maps.
 */

namespace nt
{
    /**
     * @interface InMemoryBackend
     * @brief IStorageBackend implementation that keeps all data in process memory.
     *
     * Intended for unit tests where SQLite is not appropriate. Data does not
     * survive process exit and is not thread-safe.
     */
    class NT_API InMemoryBackend : public IStorageBackend
    {
    public:
        std::string Put(std::vector<uint8_t> value) override;
        std::optional<std::vector<uint8_t>> Get(const std::string& hash) override;
        void LinkTuple(const std::vector<std::string>& relation_path,
                       const std::string& tuple_hash) override;
        std::vector<std::string> TupleHashes(const std::vector<std::string>& relation_path,
                                             std::size_t offset,
                                             std::size_t limit) override;

    private:
        std::unordered_map<std::string, std::vector<uint8_t>> kv_;
        std::unordered_map<std::string, std::vector<std::string>> index_;

        static std::string PathKey(const std::vector<std::string>& path);
    };
}
