type t

class type implementation = object
  (** Read the next tuple. [Ok None] means no tuples remain; [Error] reports a failure. *)
  method next : (Concepts.Tuple.t option, Concepts.Condition.condition) result

  (** Release the cursor's resources, even if tuples remain.
      Repeated calls are safe. After closing, [next] returns [Ok None]. *)
  method close : unit
end

val make : #implementation -> Handle.protocol
val from : Handle.t -> t Handle.interface option
val next : t Handle.interface -> (Concepts.Tuple.t option, Concepts.Condition.condition) result
val close : t Handle.interface -> unit
