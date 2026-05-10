# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Relational NT is an experimental database kernel inspired by the Windows NT kernel's object-management model. It applies explicit object management principles to database internals — relations, tuples, transactions, namespaces, permissions, cursors, and handles are first-class runtime-managed objects rather than isolated subsystems.

The project is early-stage and intentionally skeletal: headers define the architecture and document design constraints; implementations are stubs filling out as the runtime shape solidifies.

## Build Commands

**Windows (NMake):**
```
nmake /f Makefile windows
nmake /f Makefile clean
```

**Unix/Linux/macOS:**
```
make unix
make macos
make clean
```

Requires C++17. Output binary: `RNT.exe` (Windows) or `RNT` (Unix). No external dependencies beyond the standard library.

**Generate documentation:**
```
doxygen Doxyfile
```
Output lands in `docs/generated/html`. Docs are auto-published to GitHub Pages on pushes to `master`.

## Architecture

### Manager Pipeline

The system is organized as a set of single-responsibility managers. The canonical **open-handle workflow** chains them in this order:

```
HandlerManager::Open()
  → ObjectManager::Find()           — locate object by logical path
  → PermissionsManager::Access()    — capability check (READ/WRITE vs security descriptor)
  → IdentityManager::CanOpen()      — verify object supports OPEN method
  → LifecycleManager::Contention()  — detect mutability conflicts
  → LifecycleManager::Monitor()     — pin the object for the handle's lifetime
```

`HandlerManager::Close()` reverses monitoring and deallocates the handle. This explicit separation means permissions policy can evolve without touching object lookup or lifecycle code.

### Manager Responsibilities

| Manager | Role |
|---|---|
| `ObjectManager` | Central registry — stores and finds all objects (Multigroup, Relation, Tuple, Attribute, Transaction) by logical path; tracks ref counts, handle counts, security descriptors |
| `PermissionsManager` | Capability-based auth — `Firewall()` validates connection auth method; `Access()` checks object-level claims (READ/WRITE) |
| `IdentityManager` | Lifecycle eligibility — `CanOpen()` / `CanClose()` confirm an object's method set supports the requested operation |
| `LifecycleManager` | Monitoring, pinning, and contention — `Monitor()` / `Unmonitor()` track open handles; `Contention()` prevents conflicting concurrent writes; blocks history pruning when cursors hold snapshots |
| `HandlerManager` | Orchestrates the full open/close workflow above; owns handle allocation |
| `CursorManager` | Volcano-style pull iterator over storage — abstracts pagination, snapshot pinning, prefetch, buffering, and scan boundary enforcement (commit boundaries, logical branches) |
| `NamespaceReferenceManager` | Logical path mapping and namespace isolation — supports private branch shadowing (`/user_a/refs/branch/abc`); enforces all-or-nothing batching for multi-reference atomic updates |
| `VM` | Logic execution boundary — hosts two cores: **Tarski** (FOL relational calculus via Volcano iterators, guaranteed termination) and **Karuta** (SOL WAM-based higher-order logic, may not terminate); pull-based and lazy |

### Shared Types (`include/Types.h`)

- `OBJECT_TYPE`: `MULTIGROUP`, `RELATION`, `TUPLE`, `ATTRIBUTE`, `TRANSACTION`
- `METHOD`: `OPEN`, `CLOSE`, `PARSE`, `SECURITY`
- `AUTH_CLAIM`: `READ`, `WRITE`
- `AUTH_METHOD`: `CERTIFICATE`, `PLAIN_TEXT`

### Entry Points

- `include/NT.h` — main public API include
- `include/Runtime.h` — top-level `NT` facade (`IsRunning()`, `SimulateEntryCall()`)
- `src/NT.cpp` — console harness used for manual testing

### Headers as Design Record

Because implementations are intentionally minimal, the **header files are the authoritative design record**. Doxygen comments in headers explain invariants, constraints, and design rationale. Read the header before touching any `.cpp`.
