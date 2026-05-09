#pragma once

#include "Api.h"
#include "ObjectManager.h"
#include "Types.h"

#include <set>

namespace nt
{
    class NT_API PermissionsManager
    {
    public:
        // For validating connections at login
        std::set<AUTH_CLAIM> Firewall(AUTH_METHOD method);
        // For validating permission to an object
        const bool Access(const ObjectManager::registry* object, const void* connection_context);
    };
}
