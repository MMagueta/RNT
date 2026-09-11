val over :
  root:Protocols.Handle.t ->
  ?budget:int ->
  ?cancelled:(unit -> bool) ->
  unit ->
  Protocols.Context.t

val remaining : Protocols.Context.t -> int option
