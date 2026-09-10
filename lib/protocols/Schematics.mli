(** Instantiate once and share the protocol with schema providers and evaluators. *)
module type S = sig
  module Schema : Abstract.Schematics.SCHEMA
  type t

  class type implementation = object
    method schema : (Schema.t, Concepts.Condition.condition) result
  end

  val make : #implementation -> Handle.protocol
  val from : Handle.t -> t Handle.interface option
  val schema : t Handle.interface -> (Schema.t, Concepts.Condition.condition) result
  val attributes :
    t Handle.interface ->
    ((string * Schema.attribute) BatFingerTree.t, Concepts.Condition.condition) result
end

module Make (Schema : Abstract.Schematics.SCHEMA) : S with module Schema = Schema
