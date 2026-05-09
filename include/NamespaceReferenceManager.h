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
     * Design notes:
     * - Namespace isolation: allow private branch shadowing, such as
     *   /user_a/refs/branch/abc. To preserve lookup clarity, protocol-level
     *   syntax must explicitly define scope and prevent non-deterministic
     *   shadowing during resolution.
     *
     * - Multi-reference atomic updates: ensure updates to multiple references,
     *   such as pushing to master and audit_log simultaneously, are
     *   all-or-nothing. If any contention check fails, the whole batch is
     *   aborted to prevent partial commits and preserve audit consistency.
     *
     * Audit needs global access, which means it should be dissociated from a
     * branch scope. Since branches are the only contention points for
     * references, it makes sense to model audit as an independent branch too.
     */
    class NT_API NamespaceReferenceManager
    {
    };
}
