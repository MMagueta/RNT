module Make (Schema : Abstract.Schematics.SCHEMA) = struct
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
