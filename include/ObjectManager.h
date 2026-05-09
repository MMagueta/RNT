#pragma once

#include "Api.h"
#include "Types.h"

#include <cstdint>
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

        /** @brief Object representing a multigroup. */
        class Multigroup : IObject {};
        /** @brief Object representing a relation. */
        class Relation : IObject {};
        /** @brief Object representing a tuple. */
        class Tuple : IObject {};
        /** @brief Object representing a transaction. */
        class Transaction : IObject {};

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
            /** Number of object-to-object references. */
            uint32_t reference_count;
            /** Number of open handles. */
            uint32_t handle_count;
            /** Object type metadata. */
            struct object_type* type;
            /** Logical object path inside the namespace. */
            std::vector<std::string> path;
            /** security_descriptor to be added later, maybe also a set of labels */
        };

        /** @brief Node in the object registry. */
        struct registry {
            /** Shared metadata for this object. */
            struct registry_head* head;
            /** Stored object payload. */
            IObject object;
            /** TODO: Treat as a list for now, but ideally it is a tree. */
            struct registry* next;
        };

        /** Root registry entries. */
        struct registry** entries;

        /**
         * @brief Looks into this registry and attempts to retrieve an entry.
         * @param object_path Logical path to search for.
         * @return Matching registry entry, or nullptr when no object is found.
         */
        struct registry* Find(const std::vector<std::string> object_path);
    };
}
