#include "LifecycleManager.h"

namespace nt
{
    void LifecycleManager::Monitor(ObjectManager::registry* object)
    {
        if (object != nullptr)
            ++object->head->handle_count;
    }

    const bool LifecycleManager::Contention(ObjectManager::registry* object)
    {
        return object != nullptr;
    }

    void LifecycleManager::Unmonitor(ObjectManager::registry* object)
    {
        if (object != nullptr && object->head->handle_count > 0)
            --object->head->handle_count;
    }
}
