#pragma once

#include "Api.h"
#include "Types.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

/**
 * @file ObjectManager.h
 * @brief Declares the registry responsible for runtime-managed database objects.
 */

namespace nt
{
    /**
     * @brief Owns the registry used to locate and reference database objects.
     *
     * The object manager is the central directory for runtime objects. It keeps
     * the metadata needed to reason about object type, path, references, and
     * handles while the concrete storage model is still being designed.
     */
    class NT_API ObjectManager
    {
    public:
        /** @brief Base type for objects stored in the registry. */
        class IObject {
        public:
            virtual ~IObject() = default;
        };

        /** @brief Abstract registry object representing a multigroup. */
        class Multigroup : public IObject {};
        /** @brief Abstract registry object representing a relation. */
        class Relation : public IObject {};
        /** @brief Abstract registry object representing a transaction. */
        class Transaction : public IObject {};

        /** @brief Describes the behavior and capabilities of an object category. */
        struct object_type {
            /** Object category label. */
            OBJECT_TYPE label;
            /** True when the object is not durable. For example: TRANSACTION. */
            bool disposable;
            /** Methods supported by this object type. */
            std::set<METHOD> methods;
        };

        /** @brief Shared metadata stored at the head of a registry entry. */
        struct registry_head {
            // TODO: Increment reference_count when one registry object depends on another
            // (e.g. an ephemeral view that references base relations). Decrement when the
            // dependency is removed. Used to block cleanup of objects still referenced by others.
            uint32_t reference_count = 0;
            /** Number of open handles. Maintained by LifecycleManager::Monitor/Unmonitor. */
            uint32_t handle_count = 0;
            /** Object type metadata. Owned by this head. */
            std::unique_ptr<object_type> type;
            /** Logical object path inside the namespace. */
            std::vector<std::string> path;
            /** security_descriptor to be added later, maybe also a set of labels */
        };

        /** @brief Node in the object registry. */
        struct registry {
            /** Shared metadata for this object. */
            std::unique_ptr<registry_head> head;
            /** Abstract object payload. Owned by this entry. */
            std::unique_ptr<IObject> object;
            /** TODO: Treat as a list for now, but ideally it is a tree. */
            std::unique_ptr<registry> next;
        };

        /** @brief Head of the registry linked list. Owned by this manager. */
        std::unique_ptr<registry> entries;

        /**
         * @brief Registers an object under the given path, taking ownership of it.
         * @param path    Logical object path.
         * @param object  Object payload. Ownership is transferred to the registry.
         * @param type    Object type descriptor. Ownership is transferred to the registry.
         */
        void Register(std::vector<std::string> path,
                      std::unique_ptr<IObject> object,
                      std::unique_ptr<object_type> type);

        /**
         * @brief Looks into this registry and attempts to retrieve an entry.
         * @param object_path Logical path to search for.
         * @return Borrowed pointer to the matching entry, or nullptr when not found.
         */
        registry* Find(const std::vector<std::string> object_path);
    };
}
