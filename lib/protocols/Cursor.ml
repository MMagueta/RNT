module Error = struct
  open Concepts.Condition

  let not_a_cursor () =
    condition "not-a-cursor" "A handle was expected to carry the cursor protocol and did not" empty
end

type batch = {tuples: Concepts.Tuple.t BatFingerTree.t; exhausted: bool}

class type implementation = object
  method fetch : int -> (batch, Concepts.Condition.condition) result
end

type Handle.protocol += Cursor of implementation
type t = implementation

let make impl = Cursor (impl :> implementation)
let from handle = Handle.into handle (function Cursor impl -> Some impl | _ -> None)
let require handle = from handle |> Option.to_result ~none:(Error.not_a_cursor ())
let fetch i limit = Handle.invoke i (fun o -> o#fetch limit)

(* A scalar loop over a batching protocol. The buffer is per-call rather than per-cursor state, so
   [next] stays a derived convenience and an implementation still writes exactly one method: the
   cost is that a caller alternating [next] and [fetch] on one cursor sees the buffered tuples
   dropped. Callers pick one or the other. *)
let rec next i =
  let open Utilities.Result in
  (* TODO: fetch 1 and hand back its head. Because an empty non-exhausted batch means "ask again",
     this loops until it either has a tuple or sees [exhausted]. That loop is unbounded by
     construction, which is precisely the bounded-work property [fetch] has and [next] gives up --
     which is why [next] is documented for small results and scalar loops, not for scans a host
     needs to be able to cancel. *)
  let* batch = fetch i 1 in
  match BatFingerTree.front batch.tuples with
  | Some (_, tuple) -> Ok (Some tuple)
  | None -> if batch.exhausted then Ok None else next i

let drain i ?(limit = 256) () =
  let open Utilities.Result in
  (* TODO: fetch [limit] at a time, appending each batch, until [exhausted]. Appending is
     [BatFingerTree.append], which is logarithmic, so accumulating a large result does not degrade
     the way repeated list concatenation would. *)
  let rec loop tuples =
    let* batch = fetch i limit in
    let tuples = BatFingerTree.append tuples batch.tuples in
    if batch.exhausted then Ok tuples else loop tuples
  in
  loop BatFingerTree.empty
