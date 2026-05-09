#pragma once

#include "Api.h"

namespace nt
{
	/**
     * @class CursorManager
     * @brief Orchestrates data pagination from storage backends to the query executor.
     *
     * The Cursor Manager implements the glue between the 'registry' objects
     * and the Volcano-style iterator model. It translates high-level plan
     * requests into low-level backend operations (LMDB, etcd, etc.).
     *
     * DUTIES:
     * 1. Snapshot Anchoring & Pins:
     *    We need to bind the retrieval to a multigroup snapshot, the object
     *    for it should hold the reference for the tree that holds all the relations and tuples.
     *
     * 2. State Maintenance:
     *    Tracks the current position (Iterators/Offsets) within a relation
     *    to support sequential Next() calls from the algebra engine.
     *
     * 3. Prefetching & Buffering:
     *    Abstracts the 'paging' logic of the backend. For remote backends
     *    like etcd, it manages local buffers to hide latency from the executor.
     *    Think of a driver for an operating system.
     *
     * 4. Boundary Enforcement:
     *    Ensures scans do not cross commit boundaries or logical branch
     *    definitions unless explicitly requested by the plan.
     */
    class NT_API CursorManager
    {
    };
}
