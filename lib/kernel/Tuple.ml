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
  end

  type address = Concepts.Hash.hash

  (* Equal values share a storage address. *)
  let encode value =
    Concepts.Encoding.Value.bencode_of_value value |> Concepts.Encoding.Bencode.to_blob

  let decode blob =
    let open Utilities.Result in
    Concepts.Encoding.Bencode.of_blob blob |> fmap Concepts.Encoding.Value.value_of_bencode

  let bindings_of tuple =
    Concepts.Tuple.to_list tuple
    |> List.map (fun (name, value) -> name, encode value |> Concepts.Hash.hash_of_blob)

  (* Insert in name order. Splits depend on the sequence of insertions,
     so this is what keeps a tuple's tree, and hence its address, from
     depending on the order its attributes were given in. *)
  let tree tx bindings =
    let open Utilities.Result in
    List.stable_sort (fun (l, _) (r, _) -> String.compare l r) bindings
    |> List.fold_left
         (fun node (name, address) ->
           let* node = node in
           Attributes.insert tx name address node )
         (Ok Attributes.empty)

  (* A batch persists only what the node it is handed reaches, so returning
     the empty tree leaves the one we just built entirely in memory. *)
  let address_of tx tuple =
    let open Utilities.Result in
    let root = ref Attributes.empty in
    let* _ =
      Attributes.with_batch tx (fun () ->
          let* node = tree tx (bindings_of tuple) in
          root := node;
          Ok Attributes.empty )
    in
    Ok (Attributes.hash_of !root)

  (* Intermediate nodes stay in the batch. The root is persisted here
     because a batch makes no promise about a tree that reaches nothing:
     the empty tuple would otherwise be addressed but never stored. *)
  let persist tx bindings =
    let open Utilities.Result in
    let* root = Attributes.with_batch tx (fun () -> tree tx bindings) in
    Attributes.persist tx root

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
    persist tx (bindings_of tuple)

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
end
