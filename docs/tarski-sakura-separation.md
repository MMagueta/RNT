# Tarski / Sakura Separation of Concerns

This document describes how query analysis responsibilities are divided between
the **Sakura** OCaml frontend and the **Tarski** FOL runtime inside the C++ VM.

## The Two Runtimes

The VM hosts two distinct execution cores:

| Core | Language | Responsibility |
|---|---|---|
| **Tarski** | C++ (this repo) | Mechanical FOL plan execution via Volcano pull model |
| **Sakura** | OCaml | Query parsing, cardinality analysis, plan emission, physical storage |

Sakura is the authoritative source of data and of query semantics. The SQLite
backend in this repository exists only for isolated testing; in production,
CursorManager streams data from Sakura over a well-defined interface.

## What Sakura Is Responsible For

### Cardinality analysis and constraint propagation

Every relation in Sakura carries a cardinality tag: `Finite`, `ConstrainedFinite`,
`AlephZero`, or `Continuum`. When a query joins multiple relations, Sakura
propagates these tags through the plan to determine the cardinality of the result.

This analysis is non-trivial. Consider:

```karuta
natural[A], natural[B], natural[C], plus[A,B,C], eq[C, 10].
```

Each of `natural`, `plus`, and `eq` is individually `AlephZero`. But Sakura can
determine that binding `C = 10` via `eq` constrains `plus[A,B,10]` to a finite
set of natural-number pairs, making the overall result finite. Replacing
`natural` with `integer` changes the result to infinite, because negative pairs
also satisfy `plus[A,B,10]`.

Tarski does not repeat this analysis. It receives a plan that is already
annotated with cardinality metadata and executes it mechanically.

### Termination guarantees

If a query produces an infinite result set, Sakura adds a `TAKE` node to the
plan before emission. Tarski enforces the `TAKE` limit but makes no independent
judgement about whether a plan terminates.

### Join ordering

Sakura orders the children of each JOIN so that variables are bound from the
outer (left) side before the inner (right) side is probed. This ensures that
builtin relations — which may have `AlephZero` cardinality when arguments are
unbound — are always probed with fully or partially bound arguments, enabling
O(1) membership validation rather than unbounded enumeration.

### Physical storage

Sakura owns the physical storage layer. CursorManager pulls paginated tuple
streams from Sakura; the cursor abstraction hides whether the underlying data
comes from a stored relation or an ephemeral builtin generator.

## What Tarski Is Responsible For

### Mechanical plan execution

Tarski receives a complete, annotated plan tree from Sakura and executes it via
the Volcano pull model: `VM::Next(PlanNode*)` propagates demand from the root
to the leaves, returning one tuple at a time.

### Cardinality-aware execution strategy

Each `PlanNode` carries the cardinality tag Sakura attached at plan time. Tarski
uses this tag only to choose an execution strategy — never to reason about
termination:

- **Finite / ConstrainedFinite inner side** → safe to materialize for
  nested-loop join.
- **AlephZero inner side, all args bound** → probe via `membership_criteria`
  in O(1); the generator is never called.
- **AlephZero inner side, unbound args** → should not occur; Sakura must have
  added a `TAKE` node or reordered the join. Tarski may assert in debug builds.

### TAKE enforcement

`TAKE` is a first-class node in the plan tree. When `VM::Next` passes through a
`TAKE` node it decrements a counter and returns `nullptr` once the limit is
reached, regardless of what the subtree would produce.

## Why This Division Is Correct

The cardinality analysis described above requires understanding the **semantics**
of each relation — what `plus` means over naturals versus integers, how `eq`
interacts with a functional relation, and so on. That knowledge lives in Sakura.
Duplicating it in Tarski would couple two layers that must remain independent and
would place type-level reasoning inside a mechanical executor.

Tarski's contract is simple: given a safe plan, produce correct tuples. Sakura's
contract is: emit only safe plans.
