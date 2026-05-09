#pragma once

#include "Api.h"

namespace nt
{
	/**
     * @class VM
     * @brief The first-order logic (FOL) and second-order logic (SOL) dual cores
     *
     * This partitioning is here to ensure termination on retrieval with FOL.
     * 1. Tarski Runtime (FOL):
     *    A relational engine that implements Tarski's relational calculus.
     *    It uses a deterministic Volcano-iterator model to perform relational algebra operations
     *    (Joins, Projections, Selects, etc) directly against the Cursor Manager and a prepared plan.
     *
     * 2. Karuta Runtime (SOL):
     *    An isolated WAM-based environment for higher-order logic programming. It
     *    handles recursion, choice points, and other complex programming constructs
     *    that do not guarantee termination.
     *
     * @remark Lazy Evaluation & Paging Strategy
     *
     * Both the Tarski (First-Order) and Karuta (Second-Order) engines are
     * strictly pull-based. Data is streamed lazily from the Cursor Manager
     * to ensure a constant memory footprint regardless of relation size.
     *
     * PIPELINE DYNAMICS:
     * 1. Demand-Driven:
     *    The Karuta WAM only requests a fact when a goal needs satisfaction.
     *    A request propagates down the algebra tree as a chain of Next() calls.
     *
     * 2. Short-Circuiting:
     *    If the Karuta engine finds a solution or hits a failure that
     *    invalidates a branch, the Tarski iterator is discarded or reset
     *    without ever having materialized the remaining tuples.
     *
     * 3. Resource Stewardship:
     *    By maintaining laziness up to the highest logical level, we
     *    minimize pressure on the ObjectManager and keep snapshots pinned
     *    (via Monitor) only for the absolute minimum time required.
     */
    class NT_API WAM
    {
    };
}
