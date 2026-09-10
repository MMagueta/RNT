class type implementation = object
  method capabilities : string BatSet.t
  method admit : string -> (Handle.t, Concepts.Condition.condition) result
end

type Handle.protocol += Evaluator of implementation
type t = implementation

let make impl = Evaluator (impl :> implementation)
let from handle = Handle.into handle (function Evaluator impl -> Some impl | _ -> None)
let capabilities i = Handle.invoke i (fun o -> o#capabilities)
let admit i source = Handle.invoke i (fun o -> o#admit source)
