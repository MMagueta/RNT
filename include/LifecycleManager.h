#pragma once

#include "Api.h"
#include "ObjectManager.h"

/**
 * @file LifecycleManager.h
 * @brief Declares monitoring, pinning, cleanup, and contention operations.
 */

namespace nt
{
    /** @brief Manages runtime lifecycle concerns for registry objects. */
    class NT_API LifecycleManager
    {
    public:
        /**
         * @brief Starts monitoring an object's lifecycle.
         *
         * If the object is not being monitored already, monitor the lifecycle
         * by updating the handle count. Maybe also look at the reference count
         * and kill some categories of disposable objects no longer referenced
         * by other objects, like transactions.
         *
         * One use case is preventing compaction/pruning of history while
         * cursors depend on old database snapshots, for example. Think of
         * other use cases later.
         *
         * @param object Object to monitor.
         */
        void Monitor(ObjectManager::registry* object);

        /**
         * @brief Serializes contention for changes to mutable references.
         *
         * This can be used for the HEAD of a branch to serialize access. If the
         * object does not cause contention, such as a relation, then this can
         * approve the operation because relations and multigroups are immutable
         * snapshots. Transactions also do not cause contention.
         *
         * The objects that do cause contention are mutable reference states in
         * shared sessions. Examples include the HEAD of a branch, namespace
         * collisions, multi-branch atomic commits (which may damage isolation),
         * and garbage collection of old state when old-enough history is removed
         * to save space.
         *
         * @param object Object to check for contention.
         * @return True when the operation may proceed.
         */
        const bool Contention(ObjectManager::registry* object);

        /**
         * @brief Stops monitoring an object.
         *
         * The inverse of Monitor. Maybe garbage collect here also if the
         * counters go to zero.
         *
         * @param object Object to stop monitoring.
         */
        void Unmonitor(ObjectManager::registry* object);
    };
}
