#pragma once

#include "Api.h"

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
    };
}
