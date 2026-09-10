class type implementation = object
  method entry_points : string BatFingerTree.t

  method invoke :
    entry:string ->
    bindings:Concepts.Tuple.t ->
    Context.t ->
    (Handle.t, Concepts.Condition.condition) result
end

type Handle.protocol += Program of implementation
type t = implementation

let make impl = Program (impl :> implementation)
let from handle = Handle.into handle (function Program impl -> Some impl | _ -> None)
let entry_points i = Handle.invoke i (fun o -> o#entry_points)
let invoke i ~entry ~bindings ctx = Handle.invoke i (fun o -> o#invoke ~entry ~bindings ctx)
