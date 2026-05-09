#pragma once

/**
 * @file Types.h
 * @brief Shared enum types used by the runtime managers.
 */

/** @brief Runtime object categories known by the object manager. */
enum OBJECT_TYPE {
    /** A database snapshot grouping relations and tuples. */
    MULTIGROUP,
    /** A relation object. */
    RELATION,
    /** A tuple object. */
    TUPLE,
    /** An attribute object. */
    ATTRIBUTE,
    /** A transaction object. */
    TRANSACTION
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
