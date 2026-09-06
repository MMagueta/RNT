(** A tuple maps unique attribute names to values. Attribute order
    does not affect equality. *)
type t

(** The tuple with no attributes. *)
val empty : t

(** Build a tuple from named values. Duplicate names return an error. *)
val of_list : (string * Value.value) list -> (t, Condition.condition) result

(** Attribute names and values, sorted by name. *)
val to_list : t -> (string * Value.value) list

(** Attribute names in sorted order. *)
val names : t -> string list

val find : t -> string -> Value.value option
val mem : t -> string -> bool

(** The number of attributes. *)
val degree : t -> int

(** Compare tuples by their attribute names and values. *)
val equal : t -> t -> bool
