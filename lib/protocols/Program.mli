type t

class type implementation = object
  (** Available entry points. Each invocation has its own execution state,
      so the same program can be run more than once. *)
  method entry_points : string BatFingerTree.t

  (** Run [entry] with [bindings] as inputs, using the context's database state.
      The returned handle exposes the result through protocols such as [Protocols.Cursor].
      Unknown entries or invalid bindings return an error.

      The host is responsible for committing changes; evaluation never commits. *)
  method invoke :
    entry:string ->
    bindings:Concepts.Tuple.t ->
    Runtime.Context.t ->
    (Handle.t, Concepts.Condition.condition) result
end

val make : #implementation -> Handle.protocol
val from : Handle.t -> t Handle.interface option
val entry_points : t Handle.interface -> string BatFingerTree.t

val invoke :
  t Handle.interface ->
  entry:string ->
  bindings:Concepts.Tuple.t ->
  Runtime.Context.t ->
  (Handle.t, Concepts.Condition.condition) result
