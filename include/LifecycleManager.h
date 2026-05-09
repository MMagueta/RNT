#pragma once

#include "Api.h"
#include "ObjectManager.h"

namespace nt
{
    class NT_API LifecycleManager
    {
    public:
        // If object is not being monitored already, monitors the lifecycle by updating the handle count.
        // Maybe also look at the reference count and kill some categories of disposable objects no longer referenced by other objects, like transactions.
        // One use case is preventing compaction/prunning of history while cursors depend on old database snapshots, for example.
        // Think of other use cases later.
        void Monitor(ObjectManager::registry* object);
        // Serializes contention for the change of a mutable reference.
        // This can be used in, for example, the HEAD of a branch to serialize the access.
        // If the object does not causes contention, like a relation (since they are immutable, just like multigroups, all snapshots)
        // then just approve, as they do not cause any contention. Transactions also do not cause any, 
        // the only things that do are mutable reference states in shared sessions,
        // for example: the HEAD of a branch, namespace collisions (creating two branches with the same name, or two multigroups with the same name from the same state, etc),
        // multi-branch atomic commits (which I wish to not have as it damages isolation), 
        // garbage collection of old state (as in, we want to delete an old-enough history to save up space), etc.
        const bool Contention(ObjectManager::registry* object);
        // The inverse of `Monitor`, stops monitoring.
        // Maybe garbage collect here also if the counters goes to zero.
        void Unmonitor(ObjectManager::registry* object);
    };
}
