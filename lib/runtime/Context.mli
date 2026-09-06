(** An invocation's access to kernel resources. For now, this holds only
    the branch hash that selects its database state. Passing the same context
    to nested invocations keeps them on the same snapshot.

    TODO: Look up relations from the state once multigroups are implemented.
    Until then, callers supply operands directly. *)
type t

val make : state:Concepts.Hash.hash -> t

(** The database state selected for this invocation. *)
val state : t -> Concepts.Hash.hash
