type t

val this : t
val of_string : string -> t
val (@/) : string -> t -> t
val lookup : Protocols.Handle.t -> t -> (Protocols.Handle.t option, Concepts.Condition.condition) result
