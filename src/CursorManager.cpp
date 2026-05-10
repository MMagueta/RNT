#include "CursorManager.h"

#include <numeric>

namespace nt
{
    static std::string JoinPath(const std::vector<std::string>& path)
    {
        return std::accumulate(
            std::next(path.begin()), path.end(), path.front(),
            [](std::string a, const std::string& b) { return std::move(a) + "/" + b; });
    }

    CursorManager::cursor* CursorManager::Open(HandlerManager::handle* handle)
    {
        if (handle == nullptr || handle->object == nullptr)
            return nullptr;

        if (handle->object->head->type->label != RELATION)
            return nullptr;

        const std::string key = JoinPath(handle->object->head->path);
        auto it = mock_store_.find(key);
        if (it == mock_store_.end())
            return nullptr;

        auto* c = new cursor();
        c->handle = handle;
        for (const auto& row : it->second)
            c->tuples.emplace_back(row);
        return c;
    }

    Tuple* CursorManager::Next(cursor* cursor)
    {
        if (cursor == nullptr || cursor->position >= cursor->tuples.size())
            return nullptr;
        return &cursor->tuples[cursor->position++];
    }

    void CursorManager::Close(cursor* cursor)
    {
        delete cursor;
    }

    void CursorManager::MockInsert(std::vector<std::string> path, std::vector<Attribute> tuple)
    {
        mock_store_[JoinPath(path)].push_back(std::move(tuple));
    }
}
