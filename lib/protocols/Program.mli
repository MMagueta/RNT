type t

class type implementation = object
  method entry_points : string BatFingerTree.t
  method invoke :
    entry:string ->
    bindings:Concepts.Tuple.t ->
    Context.t ->
    (Handle.t, Concepts.Condition.condition) result
end

val make : #implementation -> Handle.protocol
val from : Handle.t -> t Handle.interface option
val entry_points : t Handle.interface -> string BatFingerTree.t

val invoke :
  t Handle.interface ->
  entry:string ->
  bindings:Concepts.Tuple.t ->
  Context.t ->
  (Handle.t, Concepts.Condition.condition) result
