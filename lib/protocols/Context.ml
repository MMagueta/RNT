module Error = struct
  open Concepts.Condition

  let unresolved name =
    condition "unresolved-name" "A name did not resolve to any object reachable from this context"
      ("name" |=| Concepts.Value.String name)
end

class type implementation = object
  method resolve : string -> (Handle.t option, Concepts.Condition.condition) result
  method cancelled : bool
  method charge : int -> (unit, Concepts.Condition.condition) result
  method nest : ?authority:string BatSet.t -> unit -> implementation
end

(* Unlike every other protocol here, a context is handed to [Program.invoke] directly rather than
   looked up through [Handle.into], exactly as evaluators.org's pseudocode shows. It therefore needs
   no extensible-variant constructor and [make] is a plain upcast. *)
type t = implementation

let make impl = (impl :> implementation)
let resolve context name = context#resolve name
let cancelled context = context#cancelled
let charge context n = context#charge n
let nest ?authority context = context#nest ?authority () |> make

let require context name =
  let open Utilities.Result in
  let* found = resolve context name in
  Option.to_result ~none:(Error.unresolved name) found
