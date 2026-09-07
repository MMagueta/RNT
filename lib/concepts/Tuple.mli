(** A tuple maps unique attribute names to values. Attribute order
    does not affect equality. *)
type t

(** The tuple with no attributes. *)
val empty : t

(** Build a tuple from named values. Duplicate names return an error. *)
val of_bindings : (string * Value.value) BatFingerTree.t -> (t, Condition.condition) result

(** Attribute names and values, sorted by name. *)
val bindings : t -> (string * Value.value) BatFingerTree.t

(** Attribute names in sorted order. *)
val names : t -> string BatFingerTree.t

val find : t -> string -> Value.value option
val mem : t -> string -> bool

(** The number of attributes. *)
val degree : t -> int

(** Compare tuples by their attribute names and values. *)
val equal : t -> t -> bool
