#include "MultigroupCodec.h"

#include <picosha2.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace nt::MultigroupCodec
{
    static std::vector<RelationEntry> Sorted(std::vector<RelationEntry> entries)
    {
        std::sort(entries.begin(), entries.end(),
                  [](const RelationEntry& a, const RelationEntry& b) {
                      return a.first < b.first;
                  });
        return entries;
    }

    std::string Hash(const std::vector<RelationEntry>& relations)
    {
        auto bytes = Serialize(relations);
        return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
    }

    std::vector<uint8_t> Serialize(const std::vector<RelationEntry>& relations)
    {
        std::vector<uint8_t> out;
        auto write_u32 = [&](uint32_t n) {
            out.push_back(static_cast<uint8_t>(n));
            out.push_back(static_cast<uint8_t>(n >> 8));
            out.push_back(static_cast<uint8_t>(n >> 16));
            out.push_back(static_cast<uint8_t>(n >> 24));
        };

        for (const auto& e : Sorted({ relations.begin(), relations.end() }))
        {
            write_u32(static_cast<uint32_t>(e.first.size()));
            out.insert(out.end(), e.first.begin(), e.first.end());
            write_u32(static_cast<uint32_t>(e.second.size()));
            out.insert(out.end(), e.second.begin(), e.second.end());
        }
        return out;
    }

    std::vector<RelationEntry> Deserialize(const std::vector<uint8_t>& bytes)
    {
        auto read_u32 = [&](size_t pos) -> uint32_t {
            return static_cast<uint32_t>(bytes[pos])
                 | (static_cast<uint32_t>(bytes[pos + 1]) << 8)
                 | (static_cast<uint32_t>(bytes[pos + 2]) << 16)
                 | (static_cast<uint32_t>(bytes[pos + 3]) << 24);
        };

        std::vector<RelationEntry> result;
        size_t pos = 0;
        while (pos + 8 <= bytes.size())
        {
            uint32_t name_len = read_u32(pos); pos += 4;
            std::string name(reinterpret_cast<const char*>(bytes.data() + pos), name_len);
            pos += name_len;

            uint32_t root_len = read_u32(pos); pos += 4;
            std::string root(reinterpret_cast<const char*>(bytes.data() + pos), root_len);
            pos += root_len;

            result.emplace_back(std::move(name), std::move(root));
        }
        return result;
    }

    std::string Store(IStorageBackend& store,
                      const std::vector<RelationEntry>& relations)
    {
        return store.Put(Serialize(relations));
    }
}
