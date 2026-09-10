type t

class type implementation = object
  method resolve : string -> (Handle.t option, Concepts.Condition.condition) result
  method cancelled : bool
  method charge : int -> (unit, Concepts.Condition.condition) result
  method nest : ?authority:string BatSet.t -> unit -> implementation
end

val make : #implementation -> t
val resolve : t -> string -> (Handle.t option, Concepts.Condition.condition) result
val require : t -> string -> (Handle.t, Concepts.Condition.condition) result
val cancelled : t -> bool
val charge : t -> int -> (unit, Concepts.Condition.condition) result
val nest : ?authority:string BatSet.t -> t -> t
