#include "InMemoryBackend.h"

#include <picosha2.h>
#include <numeric>

namespace nt
{
    std::string InMemoryBackend::PathKey(const std::vector<std::string>& path)
    {
        return std::accumulate(
            std::next(path.begin()), path.end(), path.front(),
            [](std::string a, const std::string& b) { return std::move(a) + "/" + b; });
    }

    std::string InMemoryBackend::Put(std::vector<uint8_t> value)
    {
        const std::string hash = picosha2::hash256_hex_string(value.begin(), value.end());
        kv_.emplace(hash, std::move(value));
        return hash;
    }

    std::optional<std::vector<uint8_t>> InMemoryBackend::Get(const std::string& hash)
    {
        auto it = kv_.find(hash);
        if (it == kv_.end())
            return std::nullopt;
        return it->second;
    }

    void InMemoryBackend::LinkTuple(const std::vector<std::string>& relation_path,
                                    const std::string& tuple_hash)
    {
        auto& hashes = index_[PathKey(relation_path)];
        for (const auto& h : hashes)
            if (h == tuple_hash) return;
        hashes.push_back(tuple_hash);
    }

    std::vector<std::string> InMemoryBackend::TupleHashes(
        const std::vector<std::string>& relation_path,
        std::size_t offset,
        std::size_t limit)
    {
        auto it = index_.find(PathKey(relation_path));
        if (it == index_.end() || offset >= it->second.size())
            return {};

        const auto& all = it->second;
        auto begin = all.begin() + static_cast<ptrdiff_t>(offset);
        auto end   = (offset + limit <= all.size())
                     ? begin + static_cast<ptrdiff_t>(limit)
                     : all.end();
        return { begin, end };
    }
}
