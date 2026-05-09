#pragma once

#include "Api.h"
#include "ObjectManager.h"

#include <string>
#include <vector>

/**
 * @file HandlerManager.h
 * @brief Declares handle allocation and release for authorized object access.
 */

namespace nt
{
    /** @brief Opens and closes authorized handles to registry objects. */
    class NT_API HandlerManager
    {
    public:
        /**
         * @brief Represents authorized access to an object in the registry.
         *
         * This increases the handle count in the object manager. We should
         * consider adding a vector to the object manager with all handles that
         * refer to an object.
         */
        struct handle {
            /** Object referenced by this handle. */
            ObjectManager::registry* object = nullptr;
            /** Connection metadata associated with the authorized access. */
            void* connection_context = nullptr;
        };

        /**
         * @brief Releases a handle allocation.
         * @param handle Handle to release.
         */
        void DeallocateHandle(struct handle* handle);

        /**
         * @brief Opens an object by path for a connection.
         * @param object_path Logical object path.
         * @param connection_context Connection metadata for the caller.
         * @return A valid handle pointer on success, or nullptr on failure.
         */
        struct handle* Open(std::vector<std::string> object_path, void* connection_context);

        /**
         * @brief Closes a previously opened handle.
         * @param handle Handle to close.
         * @return True when the handle was closed successfully.
         */
        bool Close(struct handle* handle);
    };
}
