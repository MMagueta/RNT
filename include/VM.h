#pragma once

#include "Api.h"

/**
 * @file VM.h
 * @brief Declares the logic execution boundary for relational and higher-order workloads.
 */

namespace nt
{
    /**
     * @class VM
     * @brief Hosts the first-order logic (FOL) and second-order logic (SOL) cores.
     *
     * This partitioning exists to ensure retrieval terminates when execution is
     * constrained to first-order logic.
     *
     * - Tarski runtime (FOL): a relational engine that implements Tarski's
     *   relational calculus. It uses a deterministic Volcano-iterator model to
     *   perform relational algebra operations such as joins, projections, and
     *   selects directly against the CursorManager and a prepared plan.
     *
     * - Karuta runtime (SOL): an isolated WAM-based environment for
     *   higher-order logic programming. It handles recursion, choice points, and
     *   other programming constructs that do not guarantee termination.
     *
     * @remark Lazy evaluation and paging strategy
     *
     * Both the Tarski first-order engine and the Karuta second-order engine are
     * strictly pull-based. Data is streamed lazily from the CursorManager to
     * preserve a constant memory footprint regardless of relation size.
     *
     * Pipeline dynamics:
     * - Demand-driven: the Karuta WAM requests a fact only when a goal needs
     *   satisfaction. A request propagates down the algebra tree as a chain of
     *   Next() calls.
     *
     * - Short-circuiting: if the Karuta engine finds a solution or reaches a
     *   failure that invalidates a branch, the Tarski iterator is discarded or
     *   reset without materializing the remaining tuples.
     *
     * - Resource stewardship: by maintaining laziness up to the highest logical
     *   level, the runtime minimizes pressure on ObjectManager and keeps
     *   snapshots pinned through LifecycleManager::Monitor only for the minimum
     *   time required.
     */
    class NT_API VM
    {
    };
}
