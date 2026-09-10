type t

class type implementation = object
  method capabilities : string BatSet.t
  method admit : string -> (Handle.t, Concepts.Condition.condition) result
end

val make : #implementation -> Handle.protocol
val from : Handle.t -> t Handle.interface option
val capabilities : t Handle.interface -> string BatSet.t
val admit : t Handle.interface -> string -> (Handle.t, Concepts.Condition.condition) result
