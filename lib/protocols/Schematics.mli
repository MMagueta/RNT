(** The schema component of a Schematics object. The host instantiates
    this protocol with its logical schema implementation and shares
    that module with providers and consumers. Internal schema
    operations remain values. *)
module Make (Schema : Abstract.Schematics.SCHEMA) : sig
  type t

  class type implementation = object
    method schema : (Schema.t, Concepts.Condition.condition) result
  end

  val make : #implementation -> Handle.protocol
  val from : Handle.t -> t Handle.interface option
  val schema : t Handle.interface -> (Schema.t, Concepts.Condition.condition) result

  (** List the attributes of the supplied schema, without a second
      metadata representation or any relation enumeration. *)
  val attributes :
    t Handle.interface ->
    ((string * Schema.attribute) BatFingerTree.t, Concepts.Condition.condition) result
end
