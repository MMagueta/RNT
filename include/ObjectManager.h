#pragma once

#include "Api.h"
#include "Types.h"

#include <cstdint>
#include <functional>
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

        /**
         * @brief Registry object representing a multigroup snapshot.
         *
         * merkle_root is the SHA256 hex digest of the serialized child relation
         * list (name, merkle_root) pairs in sorted order — computed by
         * MultigroupCodec::Hash and persisted via IStorageBackend::Put when the
         * snapshot is committed.
         *
         * Every commit produces a new Multigroup entry under
         * /system/snapshots/<merkle_root>. Snapshots are immutable
         * (disposable=false, exclusive=false); the rest of the system refers
         * to them by hash. A Multigroup is eligible for GC only when its
         * reference_count drops to zero (no branch HEAD, no session override,
         * no pinned cursor still references it).
         */
        class Multigroup : public IObject {
        public:
            std::string merkle_root;
        };
        /**
         * @brief Registry object representing a stored relation.
         *
         * merkle_root is the hex hash of the Merkle B-tree root node stored in
         * the KV backend.  Empty string means the relation contains no tuples.
         * Updated atomically by rnt_link_tuple / rnt_unlink_tuple / rnt_clear_relation.
         */
        class Relation : public IObject {
        public:
            std::string merkle_root;
        };
        /**
         * @brief Registry object representing an ephemeral relation.
         *
         * Ephemeral relations have no tuple storage of their own; tuples are
         * produced on demand by the generator function declared in the
         * ephemeral_object_type descriptor. They compose into a multigroup's
         * hash on equal footing with stored relations — only `merkle_root`
         * participates in MultigroupCodec::Hash, regardless of how it was
         * derived.
         *
         * `merkle_root` is the SHA256 hex digest derived from the generator's
         * identity, the current schema, and the merkle_roots of the declared
         * base relations. Any mutation to structure or rebinding of a base
         * produces a new merkle_root, which then propagates into a new
         * multigroup snapshot exactly like a tuple insertion in a stored
         * relation.
         *
         * `dependencies` lists the logical paths of base relations this
         * ephemeral relation is defined atop of. The lifecycle manager Pins
         * each entry while this object is alive, preventing GC of a stored
         * relation that still backs an ephemeral one. The dependency list is
         * also the foundation for attribute-level provenance tracking added
         * later.
         *
         * @todo Define the merkle_root composition function (hashes the
         *       generator identity + schema + sorted base merkle_roots).
         *       Until ephemeral relations are registered through a writer
         *       path this remains a documentation-only contract.
         */
        class EphemeralRelation : public IObject {
        public:
            std::string              merkle_root;
            std::vector<std::string> dependencies;
        };
        /** @brief Abstract registry object representing a transaction. */
        class Transaction : public IObject {};

        /**
         * @brief A named mutable reference to a multigroup state.
         *
         * The payload field carries the serialized multigroup bytes in whatever
         * format the caller registered them with. The C API returns these bytes
         * verbatim when a handle to the branch is opened; Sakura is responsible
         * for deserializing them into a Multigroup object.
         */
        class Branch : public IObject {
        public:
            std::string name;
            std::vector<uint8_t> payload;
        };

        /**
         * @brief Describes the behavior and capabilities of an object category.
         *
         * @todo Replace `methods` with typed callback fields: `OpenProcedure`,
         *       `CloseProcedure`, `DeleteProcedure`, and `ParseProcedure`.
         *       Store a `const char* name` label alongside each callback so that
         *       logs and assertions can identify which type fired without having
         *       to dereference a pointer. `IdentityManager::CanOpen` / `CanClose`
         *       then become thin dispatchers that invoke the corresponding
         *       procedure, or are absorbed into `HandlerManager` directly.
         *       See docs/reactos-ob-comparison.md §2.
         *
         * @todo Add an `exclusive` flag for mutable reference objects (branch
         *       HEADs, namespace entries). Non-exclusive objects — immutable
         *       snapshots, transactions — must skip contention checks entirely.
         *       See docs/reactos-ob-comparison.md §6.
         */
        struct object_type {
            /** Object category label. */
            OBJECT_TYPE label;
            /** True when the object is not durable. For example: TRANSACTION. */
            bool disposable;
            /**
             * Methods supported by this object type.
             * @todo Replace with typed callback function pointers. See class-level todo.
             */
            std::set<METHOD> methods;
            /**
             * When true, only one write-mode handle may be open at a time.
             * Applies to mutable reference states such as branch HEADs.
             * Immutable snapshot objects must leave this false.
             * @todo Wire into LifecycleManager::Contention.
             */
            bool exclusive = false;

            virtual ~object_type() = default;
        };

        /**
         * @brief Object type descriptor for EPHEMERAL_RELATION objects.
         *
         * An ephemeral relation has no physical storage. Its tuples are produced
         * on demand by a generator function. Cardinality may be finite (e.g. a
         * projected stored relation) or AlephZero (e.g. the eq/lt builtins).
         *
         * The generator receives the bound argument values that were written into
         * the cursor by the JOIN operator before each probe, together with a
         * pagination offset and limit. It must return at most `limit` tuples
         * starting at logical position `offset`.
         *
         * For membership-probe builtins (all args ground), the generator returns
         * at most one tuple. For enumeration over AlephZero relations the generator
         * maps `offset` to the appropriate pair via a bijective enumeration scheme
         * (e.g. Cantor pairing); callers must bound such scans with a TAKE node.
         */
        struct NT_API ephemeral_object_type : object_type
        {
            enum class Cardinality { Finite, ConstrainedFinite, AlephZero, Continuum };

            /**
             * @brief Produces a page of tuples for the given bound arguments.
             * @param args   Bound argument values written by the JOIN before probing.
             * @param offset Zero-based logical tuple offset (for pagination).
             * @param limit  Maximum number of tuples to return.
             */
            using Generator = std::function<std::vector<Tuple>(
                const std::vector<std::string>& args,
                std::size_t offset,
                std::size_t limit)>;

            Cardinality cardinality;
            Generator   generator;
        };

        /**
         * @brief Shared metadata stored at the head of a registry entry.
         *
         * Two independent reference counters govern object lifetime.
         * An object is eligible for garbage collection only when both reach zero:
         *
         * - `handle_count` — open sessions holding a cursor or handle on this object.
         *   Managed exclusively by LifecycleManager::Monitor / Unmonitor.
         * - `reference_count` — structural dependencies such as an ephemeral relation
         *   referencing its base relations, or a cursor pinned to an immutable snapshot
         *   version.
         *   Managed by LifecycleManager::Pin / Unpin (not yet implemented).
         *
         * All snapshot versions are immutable. An insertion or deletion produces a new
         * snapshot rather than mutating an existing one. A snapshot's `reference_count`
         * is non-zero while any cursor is pinned to it and drops to zero only when all
         * such cursors are released.
         *
         * @todo Implement `reference_count` tracking. Add LifecycleManager::Pin /
         *       Unpin. Update LifecycleManager::Unmonitor to gate GC on both counters
         *       reaching zero. See docs/reactos-ob-comparison.md §1.
         *
         * @todo Define and attach a security_descriptor. When added, store it as a
         *       pointer into a shared, content-addressed SD cache rather than inline.
         *       Permissions are strictly capability-based and must never be inherited
         *       from a parent namespace entry. See docs/reactos-ob-comparison.md §5.
         */
        struct registry_head {
            /**
             * Structural dependency count. Non-zero while another registry object
             * references this one (e.g. an ephemeral relation depending on a base
             * relation, or a cursor pinned to a snapshot version).
             * @todo Implement via LifecycleManager::Pin / Unpin.
             */
            uint32_t reference_count = 0;
            /** Number of open handles. Maintained by LifecycleManager::Monitor/Unmonitor. */
            uint32_t handle_count = 0;
            /** Object type metadata. Owned by this head. */
            std::unique_ptr<object_type> type;
            /** Logical object path inside the namespace. */
            std::vector<std::string> path;
        };

        /** @brief Node in the object registry. */
        struct registry {
            /** Shared metadata for this object. */
            std::unique_ptr<registry_head> head;
            /** Abstract object payload. Owned by this entry. */
            std::unique_ptr<IObject> object;
            /**
             * Next entry in the registry. Currently a flat linked list.
             * @todo Replace with a trie whose nodes are per-path-segment hash maps.
             *       Each directory level holds a hash map from segment string to
             *       child registry*. Find() and Register() walk the trie
             *       component-by-component instead of scanning a flat list.
             *       Consider pulling a well-tested radix-tree via vcpkg rather than
             *       implementing from scratch. See docs/reactos-ob-comparison.md §3.
             */
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
