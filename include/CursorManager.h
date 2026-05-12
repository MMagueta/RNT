#pragma once

#include "Api.h"
#include "HandlerManager.h"
#include "IStorageBackend.h"
#include "Types.h"

#include <cstddef>
#include <string>
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
     *
     * Pagination is driven by the Merkle B-tree: each cursor carries the hex
     * hash of the relation's Merkle root at open time, and LoadPage calls
     * Merkle::Page(backend, merkle_root, fetch_offset, PAGE_SIZE) to retrieve
     * the next page of tuple hashes without loading sibling subtrees.
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
         *
         * merkle_root is seeded from ObjectManager::Relation::merkle_root at
         * open time and does not change during iteration; the cursor reads the
         * snapshot captured when it was opened.
         */
        struct cursor {
            HandlerManager::handle* handle = nullptr;
            /**
             * Hex hash of the relation's Merkle B-tree root node.
             * Empty string means the relation contains no tuples.
             */
            std::string merkle_root;
            /**
             * Bound argument values for EPHEMERAL_RELATION cursors.
             * Written by the JOIN operator before each probe; ignored for
             * stored RELATION cursors.
             */
            std::vector<std::string> args;
            std::vector<Tuple> page;
            std::size_t page_position = 0;
            std::size_t fetch_offset  = 0;   // next Merkle::Page() call starts here
            bool exhausted = false;
        };

        /**
         * @brief Opens a cursor on the relation referenced by the handle.
         *
         * Fetches the first page immediately for non-empty relations. Returns
         * a valid exhausted cursor when @p merkle_root is empty (empty relation).
         * Returns nullptr when the handle does not refer to a RELATION or
         * EPHEMERAL_RELATION.
         *
         * @param handle       Open RELATION or EPHEMERAL_RELATION handle.
         * @param merkle_root  Hex hash of the relation's Merkle root at open
         *                     time. Ignored for EPHEMERAL_RELATION handles.
         */
        cursor* Open(HandlerManager::handle* handle,
                     const std::string& merkle_root = "");

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

        /**
         * Fetches a page of hashes via Merkle::Page, resolves each from the
         * KV store, and deserializes them into cursor->page. Clears the
         * existing page.  Advances fetch_offset by the number of hashes
         * returned; sets exhausted when fewer than PAGE_SIZE hashes come back.
         */
        void LoadPage(cursor* c);
    };
}
