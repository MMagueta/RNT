(** An invocation's access to kernel resources. For now, this holds only
    the branch hash that selects its database state. Passing the same context
    to nested invocations keeps them on the same snapshot.

    TODO: Look up relations from the state once multigroups are implemented.
    Until then, callers supply operands directly. *)
type t = {state: Concepts.Hash.hash}
