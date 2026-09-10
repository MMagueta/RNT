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

module Make (Schema : Abstract.Schematics.SCHEMA) = struct
  module Schema = Schema
  class type implementation = object
    method schema : (Schema.t, Concepts.Condition.condition) result
  end

  type Handle.protocol += Schematics of implementation
  type t = implementation

  let make impl = Schematics (impl :> implementation)
  let from handle = Handle.into handle (function Schematics impl -> Some impl | _ -> None)
  let schema i = Handle.invoke i (fun o -> o#schema)
  let attributes i = Result.map Schema.attributes (schema i)
end
