#pragma once
#include <cstdint>
#include <set>
#include <vector>
#include <string>

#if defined(_WIN32) && defined(NT_CONSOLE_APP)
#define NT_API
#elif defined(_WIN32) && defined(NT_EXPORTS)
#define NT_API __declspec(dllexport)
#elif defined(_WIN32)
#define NT_API __declspec(dllimport)
#else
#define NT_API
#endif

enum OBJECT_TYPE {
    MULTIGROUP,
    RELATION,
    TUPLE,
    ATTRIBUTE,
    TRANSACTION
};

enum METHOD {
    OPEN,
    CLOSE,
    PARSE, // To plug a secondary namespace
    SECURITY
};

enum AUTH_CLAIM {
    READ,
    WRITE
};

enum AUTH_METHOD {
    CERTIFICATE,
    PLAIN_TEXT
};

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
		struct registry* Find(const std::vector<std::string> object_path) { return nullptr; }
	};

    class NT_API PermissionsManager
    {
    public:
        // For validating connections at login
		std::set<AUTH_CLAIM> Firewall(AUTH_METHOD method) { return {}; }
		// For validating permission to an object
		const bool Access(const ObjectManager::registry* object, const void* connection_context) {
			// Checks the security descriptor on the head of the object we find in the ObjectManager::registry
			// Then additionally check other metadata yet to be defined in the connection_context,
			// like user group, policy, etc.
			// Yet to decide if this should be summoned at every attempt to get ahold of an object,
			// that might produce significant overhead. How can we solve this?
			return object != nullptr && connection_context != nullptr;
		}
	};

    class NT_API NamespaceReferenceManager
    {
    };
    
    class NT_API CursorManager
    {
    };
    
    class NT_API IdentityManager
    {
    public:
        // Looks into the methods of the object type and asserts if it can be opened
		const bool CanOpen(ObjectManager::registry* object) { return object != nullptr; }
		const bool CanClose(ObjectManager::registry* object) { return object != nullptr; }
	};

    class NT_API LifecycleManager
    {
    public:
        // If object is not being monitored already, monitors the lifecycle by updating the handle count.
        // Maybe also look at the reference count and kill some categories of disposable objects no longer referenced by other objects, like transactions.
        // Think of other use cases later.
		void Monitor(ObjectManager::registry* object) {}
        // Serializes contention for the change of a mutable reference.
        // This can be used in, for example, the HEAD of a branch to serialize the access.
        // If the object does not causes contention, like a relation (since they are immutable, just like multigroups, all snapshots)
        // then just approve, as they do not cause any contention. Transactions also do not cause any, 
        // the only things that do are mutable reference states in shared sessions,
        // for example: the HEAD of a branch, namespace collisions (creating two branches with the same name, or two multigroups with the same name from the same state, etc),
        // multi-branch atomic commits (which I wish to not have as it damages isolation), 
        // garbage collection of old state (as in, we want to delete an old-enough history to save up space), etc.
		const bool Contention(ObjectManager::registry* object){ return object != nullptr; }
        // The inverse of `Monitor`, stops monitoring.
        // Maybe garbage collect here also if the counters goes to zero.
		void Unmonitor(ObjectManager::registry* object) {}
    };

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
		void DeallocateHandle(struct handle* handle) {}
		struct handle Open(std::vector<std::string> object_path, void* connection_context) {
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
		bool Close(struct handle *handle) {
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
    };

    class NT_API WAM
    {
    };

    class NT_API NT
    {
    public:
        bool IsRunning() const;
        void SimulateEntryCall() {};
    };
}
