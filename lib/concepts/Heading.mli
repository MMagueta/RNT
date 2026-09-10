type t

val of_alist : (string * string) list -> t

(** attributes in name order. *)
val attributes : t -> (string * string) BatFingerTree.t

val names : t -> string BatSet.t
val arity : t -> int
val mem : string -> t -> bool

module Error : sig
  val unknown_attribute : string -> Condition.condition
  val incompatible_domain : string -> Condition.condition
end

val domain_of : string -> t -> (string, Condition.condition) result
val project : string BatFingerTree.t -> t -> (t, Condition.condition) result
val rename : (string * string) BatFingerTree.t -> t -> (t, Condition.condition) result
val shared : t -> t -> string BatSet.t
val joined : t -> t -> (t, Condition.condition) result
val compatible : t -> t -> bool

module Representation : Encoding.Record.S with type t = t
