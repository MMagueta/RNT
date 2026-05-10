#pragma once

#include "Api.h"
#include "HandlerManager.h"
#include "Types.h"

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file CursorManager.h
 * @brief Declares paging and cursor orchestration for query execution.
 */

namespace nt
{
    /**
     * @class CursorManager
     * @brief Orchestrates data pagination from storage backends to the query executor.
     *
     * The cursor manager is the glue between registry objects and the
     * Volcano-style iterator model. It translates high-level plan requests into
     * low-level backend operations such as LMDB or etcd reads.
     *
     * Responsibilities:
     * - Snapshot anchoring and pins: bind retrieval to a multigroup snapshot.
     *   The snapshot object should hold the reference to the tree containing
     *   all relations and tuples.
     *
     * - State maintenance: track the current iterator or offset inside a
     *   relation to support sequential Next() calls from the algebra engine.
     *
     * - Prefetching and buffering: abstract backend paging. For remote
     *   backends like etcd, manage local buffers to hide latency from the
     *   executor. Think of this as a database equivalent of an operating-system
     *   driver.
     *
     * - Boundary enforcement: ensure scans do not cross commit boundaries or
     *   logical branch definitions unless explicitly requested by the plan.
     */
    class NT_API CursorManager
    {
    public:
        /** @brief Active scan state for a single relation. */
        struct cursor {
            /** Handle that authorized this cursor. */
            HandlerManager::handle* handle;
            // TODO: Replace with a lazy iterator into the physical storage backend.
            // Pre-loading all tuples on Open() is a mock simplification.
            std::vector<Tuple> tuples;
            size_t position = 0;
        };

        /**
         * @brief Opens a cursor on the relation referenced by the handle.
         * @param handle Authorized handle to a RELATION object.
         * @return A new cursor, or nullptr if the object is not a RELATION.
         */
        cursor* Open(HandlerManager::handle* handle);

        /**
         * @brief Pulls the next tuple from the cursor.
         *
         * The returned pointer is valid until the next Next() or Close() call.
         *
         * @param cursor Active cursor.
         * @return Pointer to the next tuple, or nullptr when exhausted.
         */
        Tuple* Next(cursor* cursor);

        /**
         * @brief Closes the cursor and releases its resources.
         * @param cursor Cursor to close.
         */
        void Close(cursor* cursor);

        /**
         * @brief Inserts one tuple into the mock store for a given relation path.
         *
         * TODO: Replace with physical storage writes when the backend is wired up.
         *
         * @param path  Logical path of the relation.
         * @param tuple Attribute values for the tuple to insert.
         */
        void MockInsert(std::vector<std::string> path, std::vector<Attribute> tuple);

    private:
        // TODO: Replace with physical storage backend access.
        // Keyed by the relation path joined as "seg/seg/...".
        std::unordered_map<std::string, std::vector<std::vector<Attribute>>> mock_store_;
    };
}
