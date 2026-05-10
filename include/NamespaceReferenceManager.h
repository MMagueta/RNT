#pragma once

#include "Api.h"

/**
 * @file NamespaceReferenceManager.h
 * @brief Declares logical reference mapping and namespace isolation.
 */

namespace nt
{
    /**
     * @class NamespaceReferenceManager
     * @brief Handles the logical mapping and isolation of human-readable references.
     *
     * This manager is the reparse layer of the namespace. Its design mirrors the
     * ReactOS ParseProcedure / STATUS_REPARSE mechanism: a namespace entry whose
     * type carries a ParseProcedure can redirect name resolution to a different
     * path, allowing branch references and views to appear as first-class objects
     * in the namespace without exposing their physical storage paths directly.
     *
     * **Branch references.**
     * A path such as `/refs/branch/main` is a namespace entry backed by a
     * reference object. Its ParseProcedure rewrites the path to the physical
     * snapshot path (e.g. `/snapshots/abc123`). The caller's handle lands on the
     * snapshot. To advance the HEAD, the caller must explicitly open the reference
     * object itself with WRITE access, which is the sole contention point for that
     * branch.
     *
     * **View resolution.**
     * A view is an ephemeral RELATION whose ParseProcedure reparses to its base
     * relations internally on behalf of the runtime. The caller's handle and access
     * mask remain bound to the view. The base relations are accessed only via the
     * view's own type callbacks; the caller never receives a handle on them and
     * does not need explicit permission on them. Capability security is enforced at
     * the view boundary, not the base relation boundary.
     *
     * **Atomic multi-reference updates.**
     * Updating multiple references simultaneously (e.g. advancing both `main` and
     * `audit_log`) must be all-or-nothing. The implementation should acquire
     * exclusive locks on all target namespace entries before modifying any. If
     * LifecycleManager::Contention fails on any entry, all locks are released and
     * the batch is aborted. This preserves audit consistency.
     *
     * **Cycle guard.**
     * View definitions may reference other views, and branch aliases may chain.
     * Name resolution must track reparse depth and return an error after a bounded
     * number of iterations to prevent infinite cycles.
     *
     * **Audit branch.**
     * Audit requires global access dissociated from any data branch. It should be
     * modelled as an independent branch; its HEAD is the only contention point for
     * audit-affecting operations.
     *
     * @todo Implement `Resolve(path)` with a reparse loop and cycle depth guard.
     * @todo Define the reference object type with `object_type::exclusive = true`
     *       and a ParseProcedure that rewrites the resolution path.
     * @todo Define the batch-update API with all-or-nothing contention semantics.
     *       See docs/reactos-ob-comparison.md §8.
     */
    class NT_API NamespaceReferenceManager
    {
    };
}
