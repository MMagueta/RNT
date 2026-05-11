# Join Strategies in the Tarski FOL Runtime

This document covers the join operators available or planned for the Tarski
Volcano engine, their memory and I/O characteristics, and the role that backend
indexes play in making large-relation joins practical.

---

## Current implementation: nested-loop join

The current `JOIN` node is a classic nested-loop join.

```
for each outer (left) tuple L:
    reset inner (right) cursor to offset 0
    for each inner tuple R:
        if join condition holds: emit merge(L, R)
```

**Memory**: O(1). The left cursor holds one page (one tuple at `PAGE_SIZE = 1`).
The right cursor also holds one page at a time; `ResetInner` clears the page and
resets `fetch_offset` to 0 so the backend re-reads from the start for each outer
tuple. Neither side is ever materialized in full.

**I/O**: O(n × m) backend reads. The right relation is scanned once per outer
tuple, so a billion-row right side costs a billion backend reads per outer tuple.

**Best case**: ephemeral relations (builtins like `eq`, `lt`, etc.) on the right
side. These run the generator function in O(1) with no backend I/O. The join
degenerates into a streamed filter.

**Worst case**: two large stored relations with no index. Acceptable only when
one side is small, or when the right side fits in the OS page cache.

---

## Index nested-loop join

The most practical improvement. If Sakura exposes a B+ tree index on the join
attribute of the right relation, the inner loop becomes a point lookup instead
of a full scan.

```
for each outer tuple L:
    key = L[join_attr]
    R = backend.Lookup(right_relation, join_attr, key)   // O(log m)
    if R exists: emit merge(L, R)
```

**Memory**: O(1). Same as nested-loop — no buffering on either side.

**I/O**: O(n log m). Each outer tuple triggers one B+ tree traversal on the right.

**Backend requirement**: `IStorageBackend` would need a new method:

```cpp
virtual std::vector<std::string> TupleHashesByKey(
    const std::vector<std::string>& relation_path,
    const std::string& attribute,
    const std::string& value,
    std::size_t offset,
    std::size_t limit) = 0;
```

Sakura already maintains a per-relation B+ tree of tuple hashes. An additional
B+ tree keyed by attribute value is the natural extension and mirrors how most
production storage engines implement secondary indexes.

This is the correct long-term default join strategy for stored × stored joins.

---

## Sort-merge join

Both inputs are scanned once in sorted order, advancing a cursor on each side
in lock-step.

```
L = Next(left)
R = Next(right)
while L and R:
    if L[key] == R[key]: emit merge(L, R); advance both
    if L[key] <  R[key]: L = Next(left)
    if L[key] >  R[key]: R = Next(right)
```

**Memory**: O(1) if both inputs arrive pre-sorted. The Volcano model handles
page boundaries transparently — each `Next()` call fetches the next page from
the backend when the current page is exhausted, independent of the other cursor.
There is no synchronization problem at the page level; the operator logic only
ever holds two tuples at a time.

**I/O**: O(n + m). Each relation is scanned exactly once.

**Requirement**: both inputs must be sorted on the join attribute before the
merge step. This means either:
- The backend returns tuples in sorted attribute order (sorted scan support), or
- A `SORT` operator precedes each input in the plan tree.

Sorting an unsorted relation requires O(n log n) time and — unless done via an
external merge sort that stays within a fixed memory budget — O(n) memory.
Sakura's B+ tree already imposes an ordering on stored tuple hashes; whether
that ordering corresponds to an attribute value ordering depends on the index
structure.

---

## Hash join

Build a hash table from the smaller relation, probe with the larger.

```
phase 1 — build:  for each R: insert R into hash_table[R[key]]
phase 2 — probe:  for each L: for each match in hash_table[L[key]]: emit merge(L, R)
```

**Memory**: O(min(n, m)). The build side must fit entirely in memory. Fails
under memory pressure for large relations.

**I/O**: O(n + m) — each relation scanned once.

This is the right choice when the build side is small and fits in RAM. Not
suitable as a general-purpose operator in a constant-memory runtime.

---

## Grace hash join (partitioned hash join)

An extension of hash join that handles larger-than-memory relations by
partitioning both sides into buckets that individually fit in memory.

```
phase 1 — partition: hash each side into k buckets written to temporary storage
phase 2 — join:      for each bucket pair (left_i, right_i): in-memory hash join
```

**Memory**: O(√(n + m)) with optimal bucket count. Bounded but not O(1).

**I/O**: O(3(n + m)) — three full passes over the data.

Requires a writable temporary storage area (spill buffer). Relevant only if
memory pressure prevents a standard hash join and sort-merge is not available.
Not planned for the near term.

---

## Symmetric hash join

Builds hash tables for both sides simultaneously as tuples arrive from either
input. Suitable for streaming or pipelined inputs where one side may block
before producing all its tuples.

**Memory**: O(n + m) in the worst case — both hash tables may grow to full size.

Not planned. Mentioned because Sakura streams tuples lazily; if the OCaml side
ever introduces back-pressure on one relation while the other produces freely,
symmetric hashing becomes relevant.

---

## LeapFrog TrieJoin

LeapFrog TrieJoin (Veldhuizen, 2012) is the state of the art for multi-way
joins in logic programming and Datalog engines. It is **worst-case optimal**:
it runs in time proportional to the AGM bound — the tight theoretical upper
bound on the output size of a conjunctive query given the sizes of its input
relations. Traditional pairwise binary joins applied sequentially can produce
intermediate results that exceed this bound, making them suboptimal for cyclic
or highly connected queries.

### How it works

Rather than joining two relations at a time, LeapFrog TrieJoin joins all
relations in a query **simultaneously**. Each relation is represented as a
trie — a sorted, prefix-keyed index over its attribute values. One cursor per
relation is maintained, all pointing into their respective tries at the same
attribute depth.

The algorithm iterates over one shared variable at a time. At each step:

1. Find the relation whose cursor currently points to the **maximum** value
   among all cursors.
2. **Seek** all other cursors forward to that maximum (or the next value ≥ it).
3. If all cursors agree on the same value: emit the binding and descend to the
   next variable.
4. If a cursor cannot find a value ≥ the maximum: backtrack and advance the
   previous variable.

The seek operation is what makes this efficient. Instead of scanning linearly
through values that cannot match, a cursor jumps directly to the next candidate
using the sorted order of the trie. No intermediate results are materialized.

**Memory**: O(k) where k is the number of relations — one cursor per relation,
plus one tuple per active binding level. Constant with respect to relation size.

**I/O**: O(n^τ\*) where τ\* is the fractional edge cover of the query
hypergraph. For acyclic joins this matches the Yannakakis bound; for cyclic
joins it beats any binary join plan.

### Why this fits the Tarski/Karuta model

Karuta is WAM-based. The WAM already operates on term tries during unification,
advancing through sorted clause indexes when resolving goals. LeapFrog TrieJoin
is the relational analogue: the trie structure and the seek-based iteration
pattern are the same. When Sakura emits a plan to Tarski, the multi-way join
over several relations maps directly onto a LeapFrog node whose cursors are
driven by the same pull model as every other Volcano operator.

### Current state of Sakura

Reading `lib/algebra.ml`, the current join implementation materializes the
entire right-hand relation into memory before beginning the left-hand scan:

```ocaml
let right_tuples = drain right_gen in   (* ← full materialization *)
List.filter_map (fun rt -> ...) right_tuples   (* ← linear scan per left tuple *)
```

This is a documented TODO in the codebase. Beyond the memory cost, it is a
strictly binary join — three or more relations require multiple sequential
passes with growing intermediate materialization.

The generator type (`int option -> result`) is forward-only and position-counted.
It has no seek primitive: you cannot ask a generator to jump to the next value
≥ a target without scanning every value in between.

Sakura does have a B+ tree binding (`lib/bplustree.ml`, FFI to a C library) but
it is currently wired only to the KV storage layer for tuple hash persistence,
not to any per-attribute ordered index.

### What would need to change in Sakura

**1. Per-attribute sorted indexes**

Each stored relation needs a secondary index mapping attribute values to tuple
hashes, maintained in sorted order. The existing B+ tree binding is the natural
substrate for this — it already supports ordered key storage and would expose
range traversal and seek as cursor operations.

The index structure per relation would be:

```
relation_path / attribute_name → B+ tree of (value, tuple_hash_set)
```

**2. A seek-capable cursor type**

The generator type needs to be extended (or complemented) with a cursor
abstraction that supports:

- `current()` — the value at the current position
- `seek(target)` — advance to the first value ≥ target; return whether one exists
- `next()` — advance by one step

A generator that only supports `next()` cannot implement LeapFrog without
scanning every skipped value. The seek operation is the essential primitive.

**3. A multi-way join operator in `algebra.ml`**

The current `equijoin` function is binary. LeapFrog TrieJoin takes an arbitrary
list of relations and a variable ordering. A new `leapfrog_join` function would
replace the sequential application of `equijoin` for multi-relation conjunctive
queries.

### What would need to change in Tarski (this codebase)

On the C++ side, the `CursorManager` would need a `Seek` operation alongside
`Next`:

```cpp
/** Advances the cursor to the first tuple whose join-attribute value is ≥ target.
 *  Returns nullptr if no such tuple exists (relation exhausted). */
Tuple* Seek(cursor* c, const std::string& attribute, const std::string& target);
```

The plan tree would gain a `LEAPFROG` node holding N child SCAN cursors and a
variable ordering list. `VM::Next` on a `LEAPFROG` node drives the seek loop
across all cursors rather than delegating to nested-loop iteration.

---

## Summary

| Strategy | Memory | I/O | Requires |
|---|---|---|---|
| Nested-loop (current) | O(1) | O(n × m) | Nothing |
| Index nested-loop | O(1) | O(n log m) | Per-attribute B+ tree index |
| Sort-merge | O(1)* | O(n + m) | Pre-sorted inputs |
| Hash join | O(min(n,m)) | O(n + m) | Build side fits in RAM |
| Grace hash join | O(√(n+m)) | O(3(n+m)) | Spill storage |
| LeapFrog TrieJoin | O(k) | O(n^τ\*) | Seek-capable trie cursors |

\* O(1) only if both inputs arrive sorted. Sorting in-place requires O(n) memory.

The practical roadmap for this runtime:

1. **Now**: nested-loop covers all ephemeral-relation joins in O(1) memory.
2. **Next**: index nested-loop once Sakura exposes per-attribute B+ tree reads —
   covers the large stored × stored binary join case without buffering.
3. **Target**: LeapFrog TrieJoin once Sakura adds seek-capable cursors and
   per-attribute sorted indexes. This replaces both index nested-loop and
   sort-merge for multi-relation conjunctive queries and is the correct
   long-term join primitive for a Datalog/WAM-oriented engine.
4. **Only if needed**: hash join with an explicit memory budget, for ad-hoc
   joins where no usable index exists.
