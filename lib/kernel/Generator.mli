val cursor_of :
  ?on_release:(unit -> unit) ->
  (yield:(Concepts.Tuple.t -> unit) -> (unit, Concepts.Condition.condition) result) ->
  Protocols.Handle.t
