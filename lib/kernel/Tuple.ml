module Make (S : Abstract.Storage.STORAGE) = struct
  module SI = Storage.Make (S)
  module Attributes = Merkle.Make (S) (Merkle.StringKey)

  module Error = struct
    open Concepts.Condition

    let invalid_root root =
      condition "invalid-tuple-root"
        "The attribute tree of a tuple is missing from the backend. Either your storage is \
         corrupted, or this is a bug in RNT!"
        ("hash" |=| Concepts.Value.String (Concepts.Hash.to_hum_string root))

    let unknown_attribute name =
      condition "unknown-attribute" "A tuple has no such attribute"
        ("attribute" |=| Concepts.Value.String name)

    let duplicate_attribute name =
      condition "duplicate-attribute" "An attribute name occurs more than once in a tuple"
        ("attribute" |=| Concepts.Value.String name)
  end

  type address = Concepts.Hash.hash

  (* Equal values share a storage address. *)
  let encode value =
    Concepts.Encoding.Value.bencode_of_value value |> Concepts.Encoding.Bencode.to_blob

  let decode blob =
    let open Utilities.Result in
    Concepts.Encoding.Bencode.of_blob blob |> fmap Concepts.Encoding.Value.value_of_bencode

  (* Build in memory so a tuple's address is available before it is stored. *)
  let tree bindings = Attributes.build bindings

  let bindings_of tuple =
    Concepts.Tuple.to_list tuple
    |> List.map (fun (name, value) -> name, encode value |> Concepts.Hash.hash_of_blob)

  let address_of tuple = bindings_of tuple |> tree |> fst |> Attributes.hash_of

  let persist tx (root, nodes) =
    let open Utilities.Result in
    let* () =
      List.map (Attributes.persist tx) nodes |> Utilities.List.sequence |> Result.map ignore
    in
    Ok (Attributes.hash_of root)

  let node tx root =
    let open Utilities.Result in
    let* node = Attributes.find tx root in
    Option.to_result ~none:(Error.invalid_root root) node

  let value_at tx address =
    let open Utilities.Result in
    let* blob = SI.get_req tx (S.Hash address) in
    decode blob

  let store tx tuple =
    let open Utilities.Result in
    let* () =
      Concepts.Tuple.to_list tuple
      |> List.map (fun (_, value) -> SI.store_blob tx (encode value))
      |> Utilities.List.sequence
      |> Result.map ignore
    in
    persist tx (bindings_of tuple |> tree)

  let load tx root =
    let open Utilities.Result in
    let* node = node tx root in
    let* attributes =
      Attributes.fold_left tx (fun acc name address -> (name, address) :: acc) [] node
    in
    let* attributes =
      List.rev attributes
      |> List.map (fun (name, address) ->
          let* value = value_at tx address in
          Ok (name, value) )
      |> Utilities.List.sequence
    in
    Concepts.Tuple.of_list attributes

  let names tx root =
    let open Utilities.Result in
    let* node = node tx root in
    let* names = Attributes.keys tx node in
    Ok (BatFingerTree.to_list names)

  let attribute_address tx root name =
    let open Utilities.Result in
    let* node = node tx root in
    Attributes.lookup tx name node

  let attribute tx root name =
    let open Utilities.Result in
    let* address = attribute_address tx root name in
    match address with
    | None -> Ok None
    | Some address ->
        let* value = value_at tx address in
        Ok (Some value)

  (* Reuse value addresses: only the projected tree needs to be stored. *)
  let project tx root names =
    let open Utilities.Result in
    let* source = node tx root in
    let* bindings =
      List.fold_left
        (fun bindings name ->
          let* bindings = bindings in
          if List.mem_assoc name bindings then Error (Error.duplicate_attribute name)
          else
            let* address = Attributes.lookup tx name source in
            let* address = Option.to_result ~none:(Error.unknown_attribute name) address in
            Ok ((name, address) :: bindings) )
        (Ok []) names
    in
    persist tx (tree (List.rev bindings))
end
