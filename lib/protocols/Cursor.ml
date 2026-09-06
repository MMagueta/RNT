class type implementation = object
  method next : (Concepts.Tuple.t option, Concepts.Condition.condition) result
  method close : unit
end

type Handle.protocol += Cursor of implementation
type t = implementation

let make impl = Cursor (impl :> implementation)
let from handle = Handle.into handle (function Cursor impl -> Some impl | _ -> None)
let next i = Handle.invoke i (fun o -> o#next)
let close i = Handle.invoke i (fun o -> o#close)
