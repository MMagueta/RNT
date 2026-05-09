#pragma once

#include "Api.h"
#include "ObjectManager.h"
#include "Types.h"

#include <set>

/**
 * @file PermissionsManager.h
 * @brief Declares authentication and object-access checks.
 */

namespace nt
{
    /**
     * @brief Evaluates connection claims and object permissions.
     *
     * Permission checks are intentionally separated from lookup. The object
     * manager can retrieve a candidate object, while this manager decides
     * whether the caller is allowed to use it.
     */
    class NT_API PermissionsManager
    {
    public:
        /**
         * @brief Validates a connection during login.
         * @param method Authentication method used by the connection.
         * @return Claims granted to the authenticated connection.
         */
        std::set<AUTH_CLAIM> Firewall(AUTH_METHOD method);

        /**
         * @brief Validates permission to an object.
         *
         * Checks the security descriptor on the head of the object found in
         * ObjectManager::registry. It should also check connection metadata
         * that is still to be defined, such as user group and policy.
         *
         * It is still undecided whether this should run on every attempt to get
         * hold of an object, since that might produce significant overhead.
         *
         * @param object Object being accessed.
         * @param connection_context Connection metadata for the caller.
         * @return True when access is allowed.
         */
        const bool Access(const ObjectManager::registry* object, const void* connection_context);
    };
}
