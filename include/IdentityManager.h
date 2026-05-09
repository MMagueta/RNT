#pragma once

#include "Api.h"
#include "ObjectManager.h"

/**
 * @file IdentityManager.h
 * @brief Declares object capability checks.
 */

namespace nt
{
    /** @brief Checks whether an object supports identity-level operations. */
    class NT_API IdentityManager
    {
    public:
        /**
         * @brief Looks into the methods of the object type and asserts if it can be opened.
         * @param object Object to inspect.
         * @return True when the object can be opened.
         */
        const bool CanOpen(ObjectManager::registry* object);

        /**
         * @brief Checks whether an object can be closed.
         * @param object Object to inspect.
         * @return True when the object can be closed.
         */
        const bool CanClose(ObjectManager::registry* object);
    };
}
