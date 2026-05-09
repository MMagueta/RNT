#include "HandlerManager.h"

#include "IdentityManager.h"
#include "LifecycleManager.h"
#include "PermissionsManager.h"

namespace nt
{
    void HandlerManager::DeallocateHandle(struct handle* handle)
    {
    }

    struct HandlerManager::handle HandlerManager::Open(std::vector<std::string> object_path, void* connection_context)
    {
        ObjectManager objects;
        PermissionsManager permissions;
        IdentityManager identities;
        LifecycleManager lifecycles;
        ObjectManager::registry* retrieved_object = objects.Find((const std::vector<std::string>)object_path);
        if (permissions.Access(retrieved_object, (const void*)connection_context)
            && identities.CanOpen(retrieved_object)
            && lifecycles.Contention(retrieved_object)) {
            // Starts monitoring this guy
            lifecycles.Monitor(retrieved_object);
            return { retrieved_object, connection_context }; // Return a successful handle
        }
        else {
            // Return something more algebraic instead of null or an int
            return {};
        };
    }

    bool HandlerManager::Close(struct handle* handle)
    {
        if (handle == nullptr) {
            return false;
        }
        PermissionsManager permissions;
        IdentityManager identities;
        LifecycleManager lifecycles;
        // Somehow grab the object pointer and connection (maybe drop it...) from the handle
        ObjectManager::registry* object = handle->object;
        void* connection_context = handle->connection_context;
        // Checking for permissions here may be relevant.
        // What if we have a handle but no permission? Spooky
        if (permissions.Access(object, connection_context)
            && identities.CanClose(object)
            // TODO: I think that in this case we do not need to worry about contention.
            // But be careful to evaluate this scenario later.
            //&& LifecycleManager::Contention(retrieved_object)
            ) {
            // Stops monitoring this guy
            lifecycles.Unmonitor(object);
            DeallocateHandle(handle);
            return true; // Return a successful handle
        }
        else {
            // Return something more algebraic instead of null or an int.
            // It's kinda bad to not even report what went wrong.
            return false;
        };
    }
}
