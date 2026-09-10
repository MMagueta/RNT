module Make (S : Abstract.Storage.STORAGE) : sig
  type t

  val make :
    heading:Concepts.Hash.hash ->
    evaluator:string ->
    source:string ->
    entry:string ->
    bindings:Concepts.Hash.hash ->
    inputs:string BatFingerTree.t ->
    unit ->
    (t, Concepts.Condition.condition) result

  val store : S.transaction -> t -> (Concepts.Hash.hash, Concepts.Condition.condition) result
  val load : S.transaction -> S.connection -> Concepts.Hash.hash -> (Protocols.Handle.t, Concepts.Condition.condition) result
  val heading : t -> Concepts.Hash.hash
  val evaluator : t -> string
  val inputs : t -> string BatFingerTree.t

  module Error : sig
    val malformed : unit -> Concepts.Condition.condition
    val replay_not_implemented : unit -> Concepts.Condition.condition
  end

  module Representation : Concepts.Encoding.Record.S with type t = t
end
