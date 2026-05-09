#pragma once

#include "Api.h"
#include "ObjectManager.h"

namespace nt
{
    class NT_API IdentityManager
    {
    public:
        // Looks into the methods of the object type and asserts if it can be opened
        const bool CanOpen(ObjectManager::registry* object);
        const bool CanClose(ObjectManager::registry* object);
    };
}
