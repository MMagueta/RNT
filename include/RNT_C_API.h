#pragma once

/**
 * @file RNT_C_API.h
 * @brief C-callable surface over the RNT manager pipeline for OCaml ctypes.
 *
 * All types are opaque void pointers on the C side; the implementation casts
 * them to the appropriate C++ types internally. Callers must never dereference
 * handle or cursor pointers directly.
 *
 * Memory contract:
 *   - Strings returned via out-parameters (char**) are heap-allocated by the
 *     API and must be released with rnt_free_string().
 *   - Payloads returned via (uint8_t**, size_t*) are heap-allocated and must
 *     be released with rnt_free_bytes().
 *   - Handles and cursors are owned by the caller and must be closed before
 *     the program exits.
 *
 * Error convention:
 *   - Functions returning int use 0 for success and a negative value for error.
 *   - Functions returning a pointer return NULL on failure.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to a registry object opened through HandlerManager. */
typedef void* rnt_handle_t;

/** Opaque cursor over a relation, driven by the VM or used directly. */
typedef void* rnt_cursor_t;

/* ------------------------------------------------------------------ */
/* Runtime lifecycle                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialises the RNT runtime with the selected storage backend.
 *
 * Must be called once before any other API function. Subsequent calls are
 * no-ops.
 *
 * @param driver        Storage driver to use: "sqlite" or "memory".
 *                      "sqlite" persists data to @p storage_path.
 *                      "memory" keeps all data in process memory (ignores
 *                      @p storage_path); intended for tests.
 * @param storage_path  File path for the SQLite database, or ":memory:" for
 *                      an ephemeral SQLite store. Ignored when driver is
 *                      "memory".
 * @return 0 on success, negative on error.
 */
int rnt_init(const char* driver, const char* storage_path);

/* ------------------------------------------------------------------ */
/* Authentication                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Runs PermissionsManager::Firewall for a connection.
 *
 * @param auth_method  "plain_text" or "certificate".
 * @param claims_out   Set to a heap-allocated, newline-separated claim string.
 *                     Release with rnt_free_string(). NULL on error.
 * @return 0 on success, negative when authentication is rejected.
 */
int rnt_firewall(const char* auth_method, char** claims_out);

/* ------------------------------------------------------------------ */
/* Handle lifecycle                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Opens a handle to the object at the given slash-separated path.
 *
 * Runs the full HandlerManager::Open pipeline:
 * ObjectManager::Find → PermissionsManager::Access →
 * IdentityManager::CanOpen → LifecycleManager::Contention →
 * LifecycleManager::Monitor.
 *
 * @param path    Slash-separated logical path, e.g. "/system/branches/main".
 * @param claims  Claim string returned by rnt_firewall (may be NULL).
 * @return Handle pointer, or NULL when the path is not found or access is denied.
 */
rnt_handle_t rnt_open_handle(const char* path, const char* claims);

/**
 * @brief Closes a handle, running HandlerManager::Close and Unmonitor.
 * @return 0 on success, negative on error.
 */
int rnt_close_handle(rnt_handle_t handle);

/**
 * @brief Reads the raw payload bytes stored in a BRANCH object.
 *
 * Only valid for handles opened on BRANCH objects. The caller uses the bytes
 * to reconstruct the in-memory multigroup (e.g. via Sakura's deserializer).
 *
 * @param handle       Handle to a BRANCH object.
 * @param payload_out  Set to a heap-allocated copy of the payload bytes.
 *                     Release with rnt_free_bytes().
 * @param len_out      Set to the number of bytes in payload_out.
 * @return 0 on success, negative when the handle is not a BRANCH.
 */
int rnt_branch_payload(rnt_handle_t handle,
                       uint8_t**   payload_out,
                       size_t*     len_out);

/**
 * @brief Writes new payload bytes into the BRANCH object referenced by handle.
 *
 * The branch handle must have been opened with WRITE access. This updates the
 * in-memory branch object; callers are responsible for persisting the multigroup
 * state to the storage backend separately via tuple link operations.
 *
 * @param handle   Open BRANCH handle.
 * @param payload  New serialized multigroup bytes.
 * @param len      Number of bytes in payload.
 * @return 0 on success, negative on error.
 */
int rnt_branch_set_payload(rnt_handle_t handle,
                           const uint8_t* payload,
                           size_t         len);

/* ------------------------------------------------------------------ */
/* Object registration                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Registers a RELATION object at the given path.
 *
 * Idempotent: if an object already exists at the path, the call succeeds
 * without re-registering.
 *
 * @param path Slash-separated path, e.g. "/system/branches/main/relations/foo".
 * @return 0 on success, negative on error.
 */
int rnt_register_relation(const char* path);

/**
 * @brief Registers a BRANCH object at the given path with an initial payload.
 *
 * If a BRANCH already exists at this path, returns 0 without modifying it.
 *
 * @param path        Slash-separated path, e.g. "/system/branches/main".
 * @param payload     Initial serialized multigroup bytes (may be NULL for empty).
 * @param payload_len Byte count of payload.
 * @return 0 on success, negative on error.
 */
int rnt_register_branch(const char* path,
                        const uint8_t* payload,
                        size_t         payload_len);

/* ------------------------------------------------------------------ */
/* Tuple storage                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Stores a tuple and links it to the relation at the given path.
 *
 * Tuples are encoded as a flat key=value string, one attribute per line:
 *   "name=Blathers\nprofession=Museum Curator\n"
 * Attributes are sorted by name before hashing to ensure content-addressing
 * is order-independent.
 *
 * @param relation_path  Slash-separated relation path.
 * @param kv_attrs       Newline-delimited "key=value" attribute string.
 * @param hash_out       Set to the 64-character hex SHA-256 hash of the tuple.
 *                       Release with rnt_free_string(). NULL on error.
 * @return 0 on success, negative on error.
 */
int rnt_link_tuple(const char* relation_path,
                   const char* kv_attrs,
                   char**      hash_out);

/**
 * @brief Computes the Merkle root hash of all tuple hashes for a relation.
 *
 * Reads tuple hashes from the storage backend in sorted (tuple_hash ASC) order
 * and hashes them incrementally into a single root hash. The result is
 * deterministic regardless of insertion order: it depends only on the set of
 * tuples currently linked to the relation.
 *
 * The Merkle construction is a flat SHA-256 over the sorted, concatenated
 * tuple hash hex strings. For an empty relation the result is the SHA-256 of
 * an empty byte string.
 *
 * @param relation_path  Slash-separated relation path.
 * @param root_hash_out  Set to the 64-character hex root hash.
 *                       Release with rnt_free_string().
 * @return 0 on success, negative on error.
 */
int rnt_relation_merkle_root(const char* relation_path, char** root_hash_out);

/* ------------------------------------------------------------------ */
/* Cursor and VM                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Opens a cursor on the relation referenced by handle.
 *
 * The returned cursor is positioned before the first tuple. Advance it with
 * rnt_cursor_next(). The cursor must be closed with rnt_cursor_close() before
 * closing the handle.
 *
 * @param handle  Open RELATION handle.
 * @return Cursor pointer, or NULL on error.
 */
rnt_cursor_t rnt_cursor_open(rnt_handle_t handle);

/**
 * @brief Advances the cursor and returns the next tuple as a kv string.
 *
 * The tuple is encoded as a newline-delimited "key=value" string matching the
 * format accepted by rnt_link_tuple:
 *   "name=Blathers\nprofession=Museum Curator\n"
 *
 * @param cursor       Open cursor.
 * @param tuple_out    Set to a heap-allocated kv string, or NULL when exhausted.
 *                     Release with rnt_free_string() when non-NULL.
 * @return 1 when a tuple was returned, 0 when exhausted, negative on error.
 */
int rnt_cursor_next(rnt_cursor_t cursor, char** tuple_out);

/**
 * @brief Closes a cursor and releases its resources.
 * @return 0 on success, negative on error.
 */
int rnt_cursor_close(rnt_cursor_t cursor);

/* ------------------------------------------------------------------ */
/* Memory management                                                    */
/* ------------------------------------------------------------------ */

/** @brief Releases a string allocated by the API. Safe to call with NULL. */
void rnt_free_string(char* s);

/** @brief Releases a byte buffer allocated by the API. Safe to call with NULL. */
void rnt_free_bytes(uint8_t* p);

#ifdef __cplusplus
}
#endif
