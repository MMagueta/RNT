class type implementation = object
  method entry_points : string list

  method invoke :
    entry:string ->
    bindings:Concepts.Tuple.t ->
    Runtime.Context.t ->
    (Handle.t, Concepts.Condition.condition) result
end

type Handle.protocol += Program of implementation
type t = implementation

let make impl = Program (impl :> implementation)
let from handle = Handle.into handle (function Program impl -> Some impl | _ -> None)
let entry_points i = Handle.invoke i (fun o -> o#entry_points)
let invoke i ~entry ~bindings context = Handle.invoke i (fun o -> o#invoke ~entry ~bindings context)
