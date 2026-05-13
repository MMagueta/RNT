#pragma once

/**
 * @file RNT_C_API.h
 * @brief C-callable surface over the RNT manager pipeline for OCaml ctypes.
 *
 * All types are opaque void pointers on the C side; the implementation casts
 * them to the appropriate C++ types internally. Callers must never dereference
 * handle or cursor pointers directly.
 *
 * ## Memory contract
 *   - Strings returned via out-parameters (char**) are heap-allocated by the
 *     API and must be released with rnt_free_string().
 *   - Payloads returned via (uint8_t**, size_t*) are heap-allocated and must
 *     be released with rnt_free_bytes().
 *   - Handles and cursors are owned by the caller and must be closed before
 *     the program exits.
 *
 * ## Error convention
 *   - Functions returning int use 0 for success and a negative value for error.
 *   - Functions returning a pointer return NULL on failure.
 *
 * ## Thread safety
 *   This API is **not thread-safe**. The global runtime (g_rt) is a single
 *   in-process instance; all callers share the same ObjectManager and storage
 *   backend. The expected usage model is a single OCaml thread (or domain)
 *   driving the API at a time. If concurrent access is needed in the future,
 *   a per-object or per-relation mutex strategy should be introduced.
 *
 * @todo Implement AUTH_CLAIM::READ/WRITE enforcement in rnt_open_handle once
 *       PermissionsManager::Access is wired to a real policy engine. Currently
 *       all handles open with full access regardless of the claims parameter.
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
 * Must be called before any other API function. Once the runtime is
 * successfully initialised, subsequent calls are no-ops and return 0.
 * If initialisation fails (returns negative), the call may be retried
 * with corrected parameters — the runtime is left in a clean state.
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
 * Updates the in-memory branch object. Callers are responsible for persisting
 * the multigroup state to the storage backend separately via tuple link
 * operations.
 *
 * Write exclusion is structural: BRANCH objects carry @c exclusive=true in
 * their object_type, so LifecycleManager::Contention prevents a second handle
 * from being opened while a writer holds the branch. AUTH_CLAIM::WRITE is not
 * checked at the C API boundary.
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
 * @brief Removes a tuple from the relation's Merkle tree and tuple store.
 *
 * Calls Merkle::Remove on the relation's current root and updates
 * ObjectManager::Relation::merkle_root atomically.  The tuple bytes remain in
 * the KV store (they may be referenced by older snapshots); only the
 * membership in the current tree is removed.
 *
 * @param relation_path  Slash-separated relation path.
 * @param tuple_hash     64-character hex SHA-256 of the tuple to remove.
 * @return 0 on success, negative when the relation is not found.
 */
int rnt_unlink_tuple(const char* relation_path, const char* tuple_hash);

/**
 * @brief Resets a relation's Merkle root to the empty-tree state.
 *
 * Sets ObjectManager::Relation::merkle_root to the empty string.  Existing
 * tuple bytes remain in the KV store.  After this call the relation contains
 * no tuples from the perspective of cursor iteration.
 *
 * @param relation_path  Slash-separated relation path.
 * @return 0 on success, negative when the relation is not found.
 */
int rnt_clear_relation(const char* relation_path);

/**
 * @brief Returns the current Merkle root hash for a relation.
 *
 * OCaml calls this after rnt_link_tuple or rnt_unlink_tuple to read the
 * updated root back and store it in the in-memory multigroup (tree_pointer).
 * An empty string is returned for a relation that contains no tuples.
 *
 * @param relation_path  Slash-separated relation path.
 * @param root_hash_out  Set to the 64-character hex root hash, or to an empty
 *                       heap-allocated string for an empty relation.
 *                       Release with rnt_free_string().
 * @return 0 on success, negative when the relation is not found.
 */
int rnt_relation_root(const char* relation_path, char** root_hash_out);

/**
 * @brief Writes a Merkle root hash into a relation's ObjectManager entry.
 *
 * Called by Sakura's open_branch during multigroup reconstruction: after
 * deserializing the branch payload, OCaml calls this function once per
 * relation to restore the persisted Merkle root into the in-memory
 * ObjectManager so that subsequent cursors see the correct tuple set.
 *
 * @param relation_path  Slash-separated relation path.
 * @param root_hash      64-character hex Merkle root, or empty string to mark
 *                       the relation as empty.
 * @return 0 on success, negative when the relation is not found.
 */
int rnt_set_relation_root(const char* relation_path, const char* root_hash);

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
/* VM plan builder                                                      */
/* ------------------------------------------------------------------ */

/** Opaque plan node tree, built incrementally via rnt_plan_* calls. */
typedef void* rnt_plan_t;

/**
 * @brief Creates a SCAN plan node that reads all tuples from a stored relation.
 *
 * Opens a RELATION handle and cursor internally. Both are transferred to the
 * plan and released when the resulting VM cursor is closed.
 *
 * @param relation_path  Absolute slash-separated path to the relation, e.g.
 *                       "/system/branches/main/relations/public:users".
 * @return Plan node, or NULL when the relation does not exist or cannot be opened.
 */
rnt_plan_t rnt_plan_scan(const char* relation_path);

/**
 * @brief Creates a nested-loop JOIN plan node.
 *
 * Takes ownership of both @p left and @p right. On success the caller must not
 * free either child; they are released when the returned plan is freed or
 * executed. On failure (NULL return) both children are freed.
 *
 * @return Plan node, or NULL on error.
 */
rnt_plan_t rnt_plan_join(rnt_plan_t left, rnt_plan_t right);

/**
 * @brief Creates a TAKE plan node that limits output to at most @p limit tuples.
 *
 * Takes ownership of @p source. On failure @p source is freed.
 *
 * @return Plan node, or NULL on error.
 */
rnt_plan_t rnt_plan_take(rnt_plan_t source, size_t limit);

/**
 * @brief Releases a plan that was built but not yet executed.
 *
 * Closes any open cursors and handles owned by the plan tree, then frees all
 * nodes. Safe to call with NULL. Do NOT call after rnt_vm_execute_plan —
 * that function takes ownership.
 */
void rnt_plan_free(rnt_plan_t plan);

/**
 * @brief Executes a plan tree and returns a streaming VM cursor.
 *
 * Takes ownership of @p plan; the caller must not call rnt_plan_free after this.
 * The returned cursor is advanced with rnt_vm_cursor_next() and must be closed
 * with rnt_vm_cursor_close(), which also releases all plan resources.
 *
 * @return VM cursor, or NULL when the plan is NULL.
 */
rnt_cursor_t rnt_vm_execute_plan(rnt_plan_t plan);

/**
 * @brief Advances a VM cursor and returns the next merged tuple as a kv string.
 *
 * Same encoding and return-value semantics as rnt_cursor_next.
 *
 * @param vm_cursor  Cursor returned by rnt_vm_execute_plan.
 * @param tuple_out  Set to a heap-allocated kv string, or NULL when exhausted.
 *                   Release with rnt_free_string() when non-NULL.
 * @return 1 when a tuple was returned, 0 when exhausted, negative on error.
 */
int rnt_vm_cursor_next(rnt_cursor_t vm_cursor, char** tuple_out);

/**
 * @brief Closes a VM cursor, releases all plan nodes, cursors, and handles.
 * @return 0 on success, negative on error.
 */
int rnt_vm_cursor_close(rnt_cursor_t vm_cursor);

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
