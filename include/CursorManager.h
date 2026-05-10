#pragma once

#include "Api.h"
#include "HandlerManager.h"
#include "IStorageBackend.h"
#include "Types.h"

#include <cstddef>
#include <vector>

/**
 * @file CursorManager.h
 * @brief Declares paging and cursor orchestration for query execution.
 */

namespace nt
{
    /**
     * @class CursorManager
     * @brief Orchestrates paginated data retrieval from a storage backend.
     *
     * CursorManager depends only on IStorageBackend. The concrete backend
     * (SqliteBackend, InMemoryBackend, …) is injected at construction time.
     *
     * Tuples are never loaded all at once. Open() fetches the first page;
     * each subsequent Next() call transparently fetches the next page from
     * the backend once the current page is exhausted.
     */
    class NT_API CursorManager
    {
    public:
        static constexpr std::size_t PAGE_SIZE = 1;

        /** @brief Constructs a CursorManager that reads from the given backend. */
        explicit CursorManager(IStorageBackend& backend);

        /**
         * @brief Active scan state for a single relation.
         *
         * Holds one page of deserialized tuples at a time. When page_position
         * reaches the end of the page, Next() discards the page and fetches
         * the next one from the backend.
         *
         * The pointer returned by Next() is valid only until the next Next()
         * or Close() call — a page replacement invalidates all prior pointers.
         */
        struct cursor {
            HandlerManager::handle* handle = nullptr;
            std::vector<Tuple> page;
            std::size_t page_position = 0;
            std::size_t fetch_offset  = 0;   // next TupleHashes() call starts here
            bool exhausted = false;
        };

        /**
         * @brief Opens a cursor on the relation referenced by the handle.
         *
         * Fetches the first page immediately. Returns nullptr if the relation
         * has no tuples or the handle does not refer to a RELATION.
         */
        cursor* Open(HandlerManager::handle* handle);

        /**
         * @brief Pulls the next tuple from the cursor.
         *
         * Fetches the next page from the backend transparently when the
         * current page is exhausted. Returns nullptr when all pages are done.
         */
        Tuple* Next(cursor* cursor);

        /**
         * @brief Closes the cursor and releases its resources.
         * @param cursor Cursor to close.
         */
        void Close(cursor* cursor);

    private:
        IStorageBackend& backend_;

        /** Fetches a page of hashes, resolves each from the KV store, and
         *  deserializes them into cursor->page. Clears the existing page. */
        void LoadPage(cursor* c, const std::vector<std::string>& path);
    };
}
