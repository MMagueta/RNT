#pragma once

#include "Api.h"

namespace nt
{
    /**
     * @class NamespaceReferenceManager
     *
     * @brief Handles the logical mapping and isolation of human-readable references.
     *
     * DESIGN NOTES:
     * 1. Namespace Isolation:
     *    Allows for private branch shadowing (e.g., /user_a/refs/branch/abc).
     *    To maintain lookup clarity, protocol-level syntax must explicitly define
     *    the scope to prevent non-deterministic shadowing during resolution.
     *
     * 2. Multi-Ref Atomic Updates:
     *    Ensures that updates to multiple references (e.g., pushing to 'master'
     *    and 'audit_log' simultaneously) are all-or-nothing. If any single
     *    contention check fails, the entire batch is aborted to prevent
     *    partial commits and maintain consistent audit trails. Considering we 
     *    want to have a global access to audit, we would need to be dissociated
     *    from the scope of a branch, and branches are the only points of contention 
     *    for references, so it made sense to me to think of audit as an independent branch too.
     */
    class NT_API NamespaceReferenceManager
    {
    };
}
