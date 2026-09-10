type t = (string, Value.value) BatMap.t

let empty = BatMap.empty
let is_empty = BatMap.is_empty
let set = BatMap.add
let find_opt = BatMap.find_opt

module Error = struct
  open Condition

  let unknown_attribute name =
    condition "unknown-attribute" "No such attribute on this tuple"
      ("attribute" |=| Value.String name)
end

let get name tuple =
  match BatMap.find_opt name tuple with
  | Some v -> Ok v
  | None -> Error (Error.unknown_attribute name)

let attributes tuple = BatMap.foldi (fun name _ acc -> BatSet.add name acc) tuple BatSet.empty

let project names tuple =
  let open Utilities.Result in
  BatFingerTree.fold_left
    (fun acc name ->
      let* acc = acc in
      let* value = get name tuple in
      Ok (set name value acc))
    (Ok empty) names

let rename pairs tuple =
  let open Utilities.Result in
  BatFingerTree.fold_left
    (fun acc (old_name, new_name) ->
      let* acc = acc in
      let* value = get old_name tuple in
      Ok (set new_name value acc))
    (Ok empty) pairs

let merge left right =
  BatMap.foldi
    (fun name value acc ->
      match acc with
      | None -> None
      | Some acc -> (
         match BatMap.find_opt name acc with
         | None -> Some (BatMap.add name value acc)
         | Some value' -> if Value.equal value value' then Some acc else None))
    right (Some left)

(* Tuple's own wire format for a single value: kept local rather than
   added to Concepts.Encoding.Value (which only ever encodes hashes and
   options of them today) so this module owns its representation
   end to end. *)
let bencode_of_value = function
  | Value.String s -> Encoding.Bencode.Tagged ('s', Encoding.Bencode.String s)
  | Value.Integer n -> Encoding.Bencode.Tagged ('i', Encoding.Bencode.Int n)

let malformed_value () =
  Condition.condition "malformed-tuple-value"
    "A tuple field did not conform to what was expected" Condition.empty

let value_of_bencode = function
  | Encoding.Bencode.Tagged ('s', Encoding.Bencode.String s) -> Ok (Value.String s)
  | Encoding.Bencode.Tagged ('i', Encoding.Bencode.Int n) -> Ok (Value.Integer n)
  | _ -> Error (malformed_value ())

module rec Representation : (Encoding.Record.S with type t = t) = Encoding.Record.Make (Body)

and Body : Encoding.Record.BODY = struct
  type nonrec t = t

  let tag = 'T'
  let malformed = malformed_value

  let fields tuple =
    BatMap.foldi (fun name value acc -> (name, bencode_of_value value) :: acc) tuple []

  let of_fields data =
    let open Utilities.Result in
    let* kvs = Encoding.Bencode.as_dict data in
    kvs
    |> List.map (fun (name, v) -> value_of_bencode v |> Result.map (fun v -> (name, v)))
    |> Utilities.List.sequence
    |> Result.map (List.fold_left (fun acc (name, v) -> set name v acc) empty)
end

let hash tuple = Representation.to_blob tuple |> Hash.hash_of_blob
