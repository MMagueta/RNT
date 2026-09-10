type t

val empty : t
val is_empty : t -> bool
val set : string -> Value.value -> t -> t
val find_opt : string -> t -> Value.value option

module Error : sig
  val unknown_attribute : string -> Condition.condition
end

val get : string -> t -> (Value.value, Condition.condition) result
val attributes : t -> string BatSet.t
val project : string BatFingerTree.t -> t -> (t, Condition.condition) result
val rename : (string * string) BatFingerTree.t -> t -> (t, Condition.condition) result
val merge : t -> t -> t option
val hash : t -> Hash.hash

module Representation : Encoding.Record.S with type t = t
