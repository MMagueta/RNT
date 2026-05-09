#pragma once

#include "Api.h"
#include "Types.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace nt
{
    class NT_API ObjectManager
    {
    public:
        class IObject {
        public:
            virtual ~IObject() = default;
        };
        class Multigroup : IObject {};
        class Relation : IObject {};
        class Tuple : IObject {};
        class Transaction : IObject {};
        struct object_type {
            OBJECT_TYPE label;
            // If the object is not durable. E.g.: TRANSACTION
            bool disposable;
            std::set<METHOD> methods;
        };
        struct registry_head {
            uint32_t reference_count;
            uint32_t handle_count;
            struct object_type* type;
            std::vector<std::string> path;
            //security_descriptor to be added later, maybe also a set of labels
        };
        struct registry {
            struct registry_head* head;
            IObject object;
            // TODO: Treat as a list for now, but ideally it's a tree
            struct registry* next;
        };
        struct registry** entries;
        // Looks into this registry and attempts to retrieve the entry
        struct registry* Find(const std::vector<std::string> object_path);
    };
}
