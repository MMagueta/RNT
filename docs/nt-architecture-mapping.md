# NT Kernel Architecture — Holistic Mapping to a Database Kernel

A full-system view of the ReactOS NT kernel (`ntoskrnl/`) mapped onto the
architecture of a database kernel. The object manager comparison is covered
separately in `reactos-ob-comparison.md`; this document covers the rest of the
kernel and treats the project as a whole.

The central thesis: **NT is already a database kernel in disguise**. It manages
named, typed, reference-counted objects; enforces capability-based access control;
schedules work across a pool of threads; routes requests through a layered
dispatch stack; and maintains a write-buffered, lazily-flushed cache in front of
physical storage. Every one of those responsibilities has a direct counterpart in
a relational database engine.

---

## Subsystem Map

| NT subsystem | DB kernel counterpart |
|---|---|
| **KE** — kernel scheduler, IRQL, APC/DPC, synchronization primitives | Query scheduler, latch manager, deferred execution |
| **PS** — process/thread lifecycle, job objects, quotas | Session manager, worker thread pool, resource groups |
| **IO** — driver model, IRP dispatch stack, completion ports | Storage engine interface, query operator pipeline, async execution |
| **MM** — virtual memory, page faults, working set, section objects | Buffer pool, page frame allocator, eviction policy |
| **CC** — cache manager, lazy writer, VACB, prefetcher | WAL buffer / dirty page flusher, sequential prefetch |
| **SE** — tokens, ACLs, DACL/SACL, privilege evaluation | User principal, role/privilege set, access control, audit |
| **EX** — events, work queues, lookaside lists, rundown refs | Synchronization utilities, worker dispatch, buffer allocation |
| **LPC** — port objects, request/reply messaging | Wire protocol layer, client connection, inter-session RPC |
| **FSRTL** — fast I/O bypass, MCB extent map, file byte-range locks | Index hot-path, extent/page mapping, record-level locks |
| **OB** — object manager | Object manager (covered in reactos-ob-comparison.md) |
| **HAL** — CPU, interrupt, DMA, timer abstraction | Storage media abstraction, CPU/NUMA topology |
| **KD** — kernel debugger transport | EXPLAIN / trace / diagnostic interface |
| **CONFIG** — registry hive manager | System catalog, configuration store |

---

## 1. Subsystems as Client Protocols

NT supports multiple *environment subsystems*: Win32 and POSIX both run on top of
the same kernel. Each subsystem is a user-mode personality layered above a common
native API (`ntdll.dll`). The kernel itself has no awareness of Win32 or POSIX;
it exposes only native NT objects (files, processes, events). The subsystem DLLs
translate environment-specific calls into native system calls.

```
Win32 application            POSIX application
      ↓                            ↓
  kernel32.dll                  libc (POSIX layer)
      ↓                            ↓
         ntdll.dll (native NT API)
                    ↓
           ntoskrnl (kernel)
```

**Database analogue: wire protocols as subsystems.**

The database kernel (RelationalNT) is the `ntoskrnl`. Each supported wire
protocol — PostgreSQL, MySQL, HTTP/JSON — is an environment subsystem. A thin
native client library is the `ntdll` equivalent: it translates wire-protocol
messages into kernel-level handle opens, cursor operations, and VM plan
evaluations. The kernel itself never sees SQL syntax or protocol framing.

```
psql client         MySQL client        HTTP client
      ↓                  ↓                   ↓
  PG wire adapter   MySQL wire adapter   REST adapter
      ↓                  ↓                   ↓
         native client library (ntdll equivalent)
                         ↓
              RelationalNT kernel
```

The subsystem boundary is where SQL parsing, result serialization, and
protocol-specific error codes live. The kernel boundary is where object paths,
handle allocation, cursor iteration, and capability checks live. Keeping these
boundaries clean means a new wire protocol never touches the kernel; it only
implements the native-to-protocol translation layer.

`CSRSS` (the client-server runtime) is the service process that hosts
session-specific subsystem state on the NT side. The DB equivalent is a per-
protocol server daemon that owns wire connections and dispatches them to the
native API.

---

## 2. IO Manager → Query Operator Pipeline

The NT I/O manager's driver stack model is the most directly applicable pattern
for query execution.

### NT Driver Stack

```
NtReadFile
  ↓
I/O Manager: allocates IRP, fills stack location [0]
  ↓  IoCallDriver()
Filesystem driver (NTFS):  interprets path, maps to block range
  ↓  IoCallDriver()
Disk class driver: translates logical → physical sectors
  ↓  IoCallDriver()
Disk port driver: issues SCSI/ATA commands
  ↓  IoCallDriver()
Miniport: hardware-specific DMA
```

Each driver layer processes its stack location and either completes the IRP
inline (sync fast path) or pends it and calls the next driver. Completion fires
back up through registered completion routines — bottom to top — until the
original caller wakes.

### Database Analogue: PlanNode as IRP

The VM's `PlanNode` is an IRP. Each operator in the query plan is a driver layer
with a dispatch routine (`Next()`). The Volcano pull model is completion-routine
dispatch in reverse: instead of pushing results down-to-up on completion, the
caller pulls up-to-down via `Next()`.

```
VM::Next(root_plan_node)
  ↓  Next() dispatch
JOIN operator:    requests rows from left and right children
  ↓  Next()
FILTER operator:  requests rows, discards those failing predicate
  ↓  Next()
PROJECT operator: strips unwanted attributes
  ↓  Next()
SCAN operator:    pulls raw tuples from CursorManager
  ↓
CursorManager::Next() → physical storage
```

NT's `IRP_STACK_LOCATION` (one per driver layer) maps to each `PlanNode` holding
its own operator-local state (join buffer, filter expression, projection map).
NT's `IRP_MJ_READ` / `IRP_MJ_WRITE` major function codes map to `Op::SCAN`,
`Op::JOIN`, `Op::FILTER`, `Op::PROJECT`.

**Filter drivers.** NT allows filter drivers to insert between any two layers
without modifying either. The DB equivalent is transparent middleware: a
compression or encryption operator inserted between SCAN and PROJECT without
touching either. An audit operator inserted at the top of every plan without
touching the plan itself.

**Async IRP / completion ports.** NT issues an IRP and gets a completion
notification later. For the DB, a query issued by a session does not block the
session thread if the operator tree must wait on I/O. A completion-port analogue
(a result queue per session) allows the session thread to yield and be notified
when the cursor has rows ready. This is relevant when the physical storage backend
is remote (etcd, S3) and latency is non-trivial.

---

## 3. Drivers → Storage Backends

A NT device driver is a self-contained module that satisfies read/write/control
requests for a physical or logical device. The kernel does not know whether the
device is a disk, a network socket, or a USB key; it only knows the driver
dispatch table.

`CursorManager` is the equivalent of the I/O manager's dispatch layer. The mock
store (`unordered_map`) is the miniport. Replacing it with LMDB, RocksDB, etcd,
or an in-memory append log is equivalent to swapping a miniport driver without
changing the I/O manager or the filesystems above it.

```
CursorManager::Open / Next / Close
         ↓
Storage backend interface (the "driver dispatch table")
         ↓
   ┌─────────────┬──────────────┬─────────────┐
   │ LMDB backend│ etcd backend │ memory backend│
   └─────────────┴──────────────┴─────────────┘
```

The driver model implies that the storage backend interface should be stable and
narrow: `Open(path, snapshot_id)`, `Next()`, `Close()`. All backend-specific
concerns (transaction semantics, key encoding, compression) stay inside the
backend module. `CursorManager` holds snapshot pinning and buffering logic that
applies regardless of backend, exactly as the disk class driver holds caching
logic regardless of whether the miniport is SATA or NVMe.

**Filter driver analogue.** A replication backend can be a filter: it intercepts
`Next()` calls after the underlying backend returns a row and ships a copy to a
replica. A row-level encryption backend decrypts inline before returning to the
caller. These compose without the operator pipeline or the caller knowing.

---

## 4. MM → Buffer Pool

NT's Memory Manager manages physical page frames and virtual address mappings. The
three-tier hierarchy is:

```
Process working set (recently used pages, trimmed under pressure)
         ↓
Cache Manager VACBs (256 KB mapped views of files in kernel VA space)
         ↓
Physical storage (disk, via filesystem IRP)
```

Page faults are demand-load: a page is only brought from disk when first accessed.
The working set trimmer evicts least-used pages under memory pressure. Section
objects allow multiple processes to map the same physical pages (shared memory).

**Database buffer pool.** The same three tiers apply:

```
Query working set (pages referenced by active cursors, pinned)
         ↓
Buffer pool frames (fixed-size pages cached in memory)
         ↓
Physical storage backend
```

The NT page fault handler (`MmNotPresentFault`) maps to a buffer pool miss:
when a cursor requests a page not in the buffer pool, load it from the backend.
The NT working set trimmer maps to LRU eviction: under memory pressure, unpin
pages not referenced by any active cursor. NT section objects (shared physical
pages across processes) map to shared buffer pool pages accessible from multiple
sessions.

Because this project uses immutable snapshot storage, the eviction policy is
simplified: a page belonging to a snapshot pinned by at least one cursor
(`reference_count > 0` on the snapshot registry entry) cannot be evicted. Pages
belonging to snapshots with no remaining cursors are candidates for eviction
regardless of recency. This is cleaner than NT's working-set model because there
is no dirty-page complexity — writes always produce new pages, never mutate
existing ones.

---

## 5. CC → WAL and Dirty Page Flush

NT's Cache Manager sits between the filesystem and MM. Its `VACB` (Virtual Address
Control Block) is a 256 KB view of a file in kernel virtual address space. The
lazy writer (`CcWorkerThread`) periodically scans `DirtyVacbListHead` and writes
modified pages back to disk. The prefetcher traces access patterns and pre-loads
pages before they are requested.

**Database analogue: write-ahead log buffer and sequential prefetch.**

Because storage is immutable here, the cache manager's dirty-write concern does
not apply directly. New snapshot pages are always appended; they are never
modifications of existing cached pages. However, the structural lessons hold:

- **Append buffer.** New snapshot pages accumulate in an in-memory append buffer
  before being flushed to the physical backend. The lazy writer pattern (flush
  when the buffer exceeds a threshold, or periodically) applies directly.
- **Read-ahead prefetch.** Sequential scans access pages in order. A prefetch
  mechanism that issues backend reads ahead of the cursor's current position
  reduces latency. NT's prefetcher learns per-scenario patterns; a database
  prefetcher can use the query plan to predict access order exactly.
- **VACBs as buffer frames.** Each VACB is a fixed-size view; the database
  equivalent is a fixed-size buffer frame. Fixed-size frames simplify allocation
  and eviction arithmetic.

---

## 6. PS → Sessions and Worker Threads

NT process = database session. NT thread = query worker thread.

```
PEPROCESS                    Session
  ├─ Handle table              ├─ Handle table (open cursors, relations)
  ├─ Token (identity)          ├─ Connection context (AUTH_CLAIM set)
  ├─ Working set               ├─ Buffer pool pinned pages
  ├─ Quota (memory, CPU, I/O)  ├─ Resource quota (memory, CPU, I/O)
  └─ ThreadListHead            └─ Worker thread set

PETHREAD                     Query worker
  ├─ APC queue                 ├─ Deferred plan evaluation queue
  ├─ IRP queue                 ├─ Pending cursor operations
  └─ Impersonation context     └─ SECURITY DEFINER / SET ROLE context
```

NT job objects group processes with shared limits: NT's `NtAssignProcessToJobObject`
enforces memory, CPU time, and I/O quotas across a group. The DB equivalent is a
*resource group* or *tenant*: a set of sessions sharing a memory limit and a CPU
share. Multi-tenant databases need this to prevent one tenant's runaway query from
starving others. Job objects are the right model: assign sessions to resource
groups at connection time and enforce limits at the job-object level rather than
per-session.

**Impersonation.** NT allows a thread to temporarily assume a different security
context (`SeImpersonateClient`). The DB equivalent is `SET ROLE` or `SECURITY
DEFINER` in stored procedures: the query worker temporarily adopts a different
access context without requiring the connection to re-authenticate.

---

## 7. SE → Authorization Beyond the Object Manager

NT's security subsystem extends what the object manager does. Where OB attaches a
security descriptor to an object and checks access at handle-open time, SE
provides the richer infrastructure underneath:

- **Token structure.** A token carries the caller's SID (identity), group
  memberships, privilege set, and mandatory integrity level. The DB equivalent
  is the connection context: user identity, granted AUTH_CLAIMs, session-level
  role, and a trust level (e.g., a connection from localhost vs. from the public
  internet might carry different trust).
- **DACL / SACL split.** The DACL controls access; the SACL controls audit logging
  (which accesses are recorded, on success or failure). For a database these are
  different concerns: the permission check decides whether to allow the operation;
  the audit policy decides whether to log it. Conflating them in a single
  security descriptor is correct design; it keeps both concerns co-located with
  the object rather than scattered.
- **Privilege evaluation.** NT privileges (SE_BACKUP_PRIVILEGE bypasses ACLs;
  SE_DEBUG_PRIVILEGE allows opening any process) have a DB analogue in superuser
  privileges: a DBA role that bypasses row-level security, or a replication user
  that bypasses normal write contention checks. These are orthogonal to the
  capability tree and should be modelled as privileges on the connection token,
  not as explicit capabilities on every object.
- **Mandatory access control (integrity levels).** NT's integrity levels
  (UNTRUSTED < LOW < MEDIUM < HIGH < SYSTEM) prevent low-integrity processes
  from writing to high-integrity objects even if the DACL permits it. The DB
  equivalent is a trust tier on connections: an externally-facing API connection
  at LOW tier cannot write to audit or system catalog tables regardless of the
  DACL, because those tables require HIGH tier.

---

## 8. LPC → Wire Protocol and Inter-Session Messaging

NT's Local Procedure Call provides synchronous request-reply messaging between
processes via named port objects. A server process creates a named connection port;
clients connect and receive private communication ports. This is the mechanism
through which Win32 applications communicate with CSRSS (console, shutdown) and
through which subsystem DLLs synchronize session state with the subsystem server.

**Database analogue: wire protocol connection.**

Each client connection to the database is an LPC communication port. The session
server daemon listens on a named port (the TCP socket); accepted connections
become private communication ports (per-client sessions). The session daemon is
CSRSS.

LPC's fixed message size limit (`LPCP_MAX_MESSAGE_SIZE`) prevents a misbehaving
client from forcing unbounded allocation. The DB wire protocol layer should apply
the same discipline: a maximum query length, a maximum result frame size, and a
maximum number of concurrent open cursors per session.

LPC ports are objects in the NT namespace. Database connection endpoints (the
listening socket, or a named pipe for local connections) should similarly be
registered as objects in the RelationalNT namespace, making them subject to the
same capability checks: a client must have explicit permission to connect to a
particular endpoint.

---

## 9. FSRTL → Index Extent Management and Record Locks

The File System Runtime Library is a set of utilities that filesystems use. The
most relevant pieces for a database are:

- **MCB (Map Control Block).** Tracks the logical-to-physical block mapping for a
  file that may be stored in non-contiguous extents on disk. The DB equivalent is
  an index page map: a B-tree or extent map translating a logical page number in
  an index to a physical storage location. The MCB API (`FsRtlAddLargeMcbEntry`,
  `FsRtlLookupLargeMcbEntry`) is a model for an extent manager that handles
  fragmentation without surfacing it to callers.
- **Fast I/O bypass.** `FAST_IO_DISPATCH` provides function pointers for
  synchronous I/O that skip IRP allocation entirely when the requested data is
  already in the cache. The DB equivalent is a buffer pool hot-path: if the
  requested snapshot page is already pinned in the buffer pool, return it without
  going through the full CursorManager dispatch chain.
- **Byte-range file locking.** `FILE_LOCK` tracks exclusive/shared locks on
  sub-file ranges. The DB equivalent is record-level locking within a relation.
  Since storage here is immutable, this concern is narrowed to locking mutable
  references (branch HEADs) rather than individual records — but the MCB-style
  range-tracking structure is still the right data structure for knowing which
  parts of a namespace are currently locked by which writers.

---

## 10. HAL → Storage Media Abstraction

The Hardware Abstraction Layer isolates the kernel from CPU, interrupt controller,
and timer differences between x86 and ARM machines. It provides a stable set of
primitives (`HalAllocateAdapterChannel`, `HalReadDmaCounter`, `KeAcquireSpinLock`)
that the rest of the kernel calls without knowing the underlying hardware.

**Database analogue: storage media and topology abstraction.**

The equivalent boundary sits between `CursorManager` (the "kernel") and the
physical storage backend (the "hardware"). The HAL lessons:

- **Platform-neutral primitives.** `CursorManager::Next()` and `Close()` should
  never contain NVMe-specific or LMDB-specific code. Those specifics live in the
  backend module, exactly as NVMe DMA specifics live in the miniport, not the HAL.
- **CPU/NUMA topology.** HAL exposes processor and memory node topology so the
  scheduler can make NUMA-aware placement decisions. A database on a NUMA machine
  benefits from binding buffer pool regions and worker threads to the same NUMA
  node. This is a future concern but the right interface is at the HAL boundary:
  a `platform_topology` structure that the scheduler queries without knowing
  whether it is running on x86, ARM, or a cloud VM.
- **Timer abstraction.** HAL provides a system tick and a high-resolution
  performance counter. The DB needs both: a coarse timer for transaction
  timestamps and TTL-based expiry, and a high-resolution counter for query
  latency measurement.

---

## 11. KD → Diagnostic Interface

NT's kernel debugger (`kd/`) provides a serial transport over which WinDBG can
inspect kernel state, set breakpoints, and walk data structures live. The
`!object`, `!handle`, and `!poolused` debugger extensions expose the object
manager, handle tables, and pool allocator without stopping the machine.

**Database analogue: EXPLAIN and live diagnostic.**

The equivalent is a diagnostic interface that can walk live runtime state: the
object registry, open handle table, active cursors, and lifecycle counters.
Implemented as a reserved namespace path (`/system/diagnostics/`), it exposes
read-only virtual objects whose `Next()` returns runtime state rather than stored
data. Queries against `/system/diagnostics/cursors` return a row per open cursor;
queries against `/system/diagnostics/objects` return a row per registry entry with
its handle and reference counts. This is `EXPLAIN ANALYZE` and
`pg_stat_activity` unified under the same object model, for free.

---

## 12. Config Manager → System Catalog

NT's configuration manager (`config/`) manages registry hives — persistent
key-value stores loaded at boot and kept consistent via transaction logs. The
registry is itself stored as a structured binary file that the kernel reads before
any filesystem driver is loaded.

**Database analogue: system catalog.**

The system catalog is a set of relations stored in a reserved multigroup
(`/system/catalog/`) that describes all other objects: their types, paths,
security descriptors, and capabilities. It is the registry of the database. It
must be readable before any user-defined relations are accessible, exactly as the
registry is readable before any device driver is loaded.

Because storage is immutable here, the catalog is itself versioned: each schema
change produces a new catalog snapshot, and the kernel always reads the latest
committed catalog version on startup. Old catalog versions remain accessible for
historical schema reconstruction (e.g., reading data written under a previous
schema). This is more powerful than the NT registry, which has no built-in
history.

---

## Architectural Summary

The full NT kernel, mapped to a database, produces this layer diagram:

```
┌─────────────────────────────────────────────────────────────────┐
│              Wire Protocol Subsystems (LPC / Env. Subsystems)   │
│    PostgreSQL wire   MySQL wire   HTTP/JSON   native client lib  │
├─────────────────────────────────────────────────────────────────┤
│                   Session Manager  (PS)                         │
│    Sessions · Worker threads · Resource groups · Impersonation  │
├─────────────────────────────────────────────────────────────────┤
│              Authorization Engine  (SE + OB)                    │
│    Tokens · Capabilities · DACL/SACL · Privilege evaluation     │
├─────────────────────────────────────────────────────────────────┤
│                Query Operator Pipeline  (IO / VM)               │
│    SCAN · FILTER · PROJECT · JOIN · SORT · AGGREGATE            │
│    Async execution · Filter-driver middleware · Plan nodes       │
├─────────────────────────────────────────────────────────────────┤
│     Object Manager  (OB)          Namespace  (NamespaceRefMgr)  │
│     Registry · Handles · Types    Branch refs · View reparse    │
├─────────────────────────────────────────────────────────────────┤
│            Buffer Pool  (MM + CC)                               │
│    Page frames · Working set · Prefetch · Append flusher        │
├─────────────────────────────────────────────────────────────────┤
│              Storage Backend Interface  (IO driver model)       │
│    CursorManager · Backend dispatch table · Filter backends     │
├─────────────────────────────────────────────────────────────────┤
│         Physical Backends  (HAL / miniport drivers)             │
│         LMDB · RocksDB · etcd · in-memory · S3                 │
└─────────────────────────────────────────────────────────────────┘
```

The current RelationalNT codebase covers the Object Manager, parts of the
Operator Pipeline (SCAN only), and the authorization skeleton. The layers above
(session manager, wire protocols) and below (real buffer pool, real storage
backends) are the next frontier. The NT architecture shows that each layer has a
well-understood responsibility and a narrow interface to its neighbours — that
discipline is the reason the NT kernel scales from embedded devices to 1024-core
servers without architectural changes.
