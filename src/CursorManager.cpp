#include "CursorManager.h"
#include "ObjectManager.h"
#include "TupleCodec.h"

namespace nt
{
    CursorManager::CursorManager(IStorageBackend& backend)
        : backend_(backend)
    {}

    void CursorManager::LoadPage(cursor* c, const std::vector<std::string>& path)
    {
        auto hashes = backend_.TupleHashes(path, c->fetch_offset, PAGE_SIZE);
        c->page.clear();
        c->page_position = 0;

        for (const auto& hash : hashes)
        {
            auto bytes = backend_.Get(hash);
            if (!bytes) continue;
            c->page.emplace_back(TupleCodec::Deserialize(*bytes));
        }

        c->fetch_offset += hashes.size();
        if (hashes.size() < PAGE_SIZE)
            c->exhausted = true;
    }

    CursorManager::cursor* CursorManager::Open(HandlerManager::handle* handle)
    {
        if (handle == nullptr || handle->object == nullptr)
            return nullptr;

        const auto label = handle->object->head->type->label;

        if (label == EPHEMERAL_RELATION)
        {
            // Cursor is created exhausted — the JOIN operator writes args and
            // resets the state before the first probe.
            auto* c = new cursor();
            c->handle = handle;
            c->exhausted = true;
            return c;
        }

        if (label != RELATION)
            return nullptr;

        auto* c = new cursor();
        c->handle = handle;
        LoadPage(c, handle->object->head->path);

        if (c->page.empty())
        {
            delete c;
            return nullptr;
        }
        return c;
    }

    Tuple* CursorManager::Next(cursor* c)
    {
        if (c == nullptr) return nullptr;

        if (c->page_position < c->page.size())
            return &c->page[c->page_position++];

        if (c->exhausted) return nullptr;

        if (c->handle->object->head->type->label == EPHEMERAL_RELATION)
        {
            auto* etype = static_cast<ObjectManager::ephemeral_object_type*>(
                c->handle->object->head->type.get());
            c->page = etype->generator(c->args, c->fetch_offset, PAGE_SIZE);
            c->page_position = 0;
            c->fetch_offset += c->page.size();
            if (c->page.size() < PAGE_SIZE)
                c->exhausted = true;
            if (c->page.empty()) return nullptr;
            return &c->page[c->page_position++];
        }

        LoadPage(c, c->handle->object->head->path);
        if (c->page.empty()) return nullptr;
        return &c->page[c->page_position++];
    }

    void CursorManager::Close(cursor* c)
    {
        delete c;
    }
}
