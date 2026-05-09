#pragma once

#include "Api.h"
#include "ObjectManager.h"

#include <string>
#include <vector>

namespace nt
{
    class NT_API HandlerManager
    {
    public:
        // This is supposed to represent an authorized access to a certain object in the registry
        // This increases the handle count in the object manager
        // We should consider adding on the object manager a vector of all the handles that refer to an object
        struct handle {
            ObjectManager::registry* object = nullptr;
            void* connection_context = nullptr;
        };
        void DeallocateHandle(struct handle* handle);
        struct handle Open(std::vector<std::string> object_path, void* connection_context);
        bool Close(struct handle* handle);
    };
}
