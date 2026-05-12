#pragma once

#include <string>
#include <vector>

/**
 * @file Types.h
 * @brief Shared enum types and core data types used by the runtime.
 */

/** @brief Runtime object categories known by the object manager. */
enum OBJECT_TYPE {
    /** A database snapshot grouping relations and tuples. */
    MULTIGROUP,
    /** A relation object backed by physical storage. */
    RELATION,
    /** An attribute object (schema metadata). */
    ATTRIBUTE,
    /** A transaction object. */
    TRANSACTION,
    /**
     * A relation whose tuples are produced on demand by a generator function
     * rather than read from physical storage. Cardinality may be finite (e.g.
     * a projected stored relation) or AlephZero (e.g. the eq builtin). The
     * object_type for this label must be an ephemeral_object_type.
     */
    EPHEMERAL_RELATION,
    /**
     * A named mutable reference to a multigroup state, analogous to a git
     * branch. A BRANCH object carries an opaque payload (serialized multigroup
     * bytes) that the caller — in practice Sakura — deserializes on open.
     *
     * Branch objects are the entry point for database connections: a client
     * opens a handle to /system/branches/<name>, reads the payload bytes to
     * reconstruct the multigroup in memory, and holds the handle for the
     * duration of the session.
     *
     * The `exclusive` flag on the object_type should be set to true so that
     * LifecycleManager::Contention serializes concurrent writers.
     */
    BRANCH
};

/** @brief Operations that may be supported by an object type. */
enum METHOD {
    /** Opens an object and creates an authorized handle. */
    OPEN,
    /** Closes a previously opened handle. */
    CLOSE,
    /** Parses through a secondary namespace. */
    PARSE,
    /** Performs security checks. */
    SECURITY
};

/** @brief Claims granted to an authenticated connection. */
enum AUTH_CLAIM {
    /** Permission to read object state. */
    READ,
    /** Permission to mutate object state. */
    WRITE
};

/** @brief Authentication strategies supported by the permissions layer. */
enum AUTH_METHOD {
    /** Certificate-based authentication. */
    CERTIFICATE,
    /** Plain-text authentication. */
    PLAIN_TEXT
};

namespace nt
{
    /** @brief A single named value in a tuple, produced by the cursor layer. */
    struct Attribute {
        std::string name;
        std::string value;
    };

    /**
     * @brief A single row of data streamed lazily from the cursor layer.
     *
     * Follows the Volcano pull model: callers repeatedly call Next() to consume
     * one attribute at a time. Returns nullptr when all attributes are exhausted.
     */
    class Tuple {
    public:
        explicit Tuple(std::vector<Attribute> attributes)
            : attributes_(std::move(attributes)) {}

        /** @brief Returns the next attribute, or nullptr when exhausted. */
        const Attribute* Next() {
            if (position_ < attributes_.size())
                return &attributes_[position_++];
            return nullptr;
        }

        void Reset() { position_ = 0; }

        /** @brief Direct read-only access to all attributes without affecting position. */
        const std::vector<Attribute>& attrs() const { return attributes_; }

    private:
        std::vector<Attribute> attributes_;
        size_t position_ = 0;
    };
}
