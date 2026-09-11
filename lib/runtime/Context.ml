module Error = struct
  open Concepts.Condition

  let budget_exhausted ~requested ~remaining =
    condition "budget-exhausted" "An evaluation asked for more budget than it had left"
      ("requested" |=| Concepts.Value.Integer requested
      & "remaining" |=| Concepts.Value.Integer remaining)

  let not_a_directory segment =
    condition "not-a-directory" "A path segment named an object with no Directory protocol"
      ("segment" |=| Concepts.Value.String segment)
end

type budget = int Atomic.t

let charge_budget budget n =
  let open Utilities.Result in
  let* _ =
    Utilities.Atomic.mswap budget (fun remaining ->
        if remaining >= n then Ok (remaining - n)
        else Error (Error.budget_exhausted ~requested:n ~remaining) )
  in
  Ok ()

class context ~root ~budget ~cancelled =
  object (self)
    val root : Protocols.Handle.t = root
    val budget : budget = budget
    val cancelled : unit -> bool = cancelled

    method resolve (name : string) :
        (Protocols.Handle.t option, Concepts.Condition.condition) result =
      let open Utilities.Result in
      let rec walk handle = function
        | [] -> Ok (Protocols.Handle.copy handle)
        | segment :: rest ->
            let* directory = Protocols.Directory.from handle
              |> Option.to_result ~none:(Error.not_a_directory segment)
            in
            let* found = Protocols.Directory.find directory segment in
            match found with
            | None -> Ok None
            | Some child ->
                Fun.protect ~finally:(fun () -> Protocols.Handle.release child)
                  (fun () -> walk child rest)
      in
      String.split_on_char '/' name |> List.filter (fun segment -> segment <> "") |> walk root

    method cancelled = cancelled ()
    method charge n = charge_budget budget n
    method nest ?authority:(_ : string BatSet.t option) () =
      (new context ~root ~budget ~cancelled :> Protocols.Context.implementation)

    initializer ignore self
  end

let over ~root ?(budget = max_int) ?(cancelled = fun () -> false) () =
  Protocols.Context.make (new context ~root ~budget:(Atomic.make budget) ~cancelled)

let remaining (_ : Protocols.Context.t) : int option = failwith "not implemented"
