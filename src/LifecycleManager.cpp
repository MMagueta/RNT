#include "LifecycleManager.h"

namespace nt
{
    void LifecycleManager::Monitor(ObjectManager::registry* object)
    {
    }

    const bool LifecycleManager::Contention(ObjectManager::registry* object)
    {
        return object != nullptr;
    }

    void LifecycleManager::Unmonitor(ObjectManager::registry* object)
    {
    }
}
