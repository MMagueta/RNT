module Make (S : Abstract.Storage.STORAGE) = struct
  module SI = Storage.Make (S)
  module Attributes = Merkle.Make (S) (Merkle.StringKey)
  open Utilities.Result

  type address = Concepts.Hash.hash

  let encode value =
    Concepts.Encoding.Value.bencode_of_value value |> Concepts.Encoding.Bencode.to_blob

  let tree tx intern tuple =
    BatFingerTree.fold_left
      (fun node (name, value) ->
        let* node = node in
        let* address = intern (encode value) in
        Attributes.insert tx name address node )
      (Ok Attributes.empty) (Concepts.Tuple.bindings tuple)

  let address_of tx tuple =
    let root = ref Attributes.empty in
    (* The existing batch API flushes only the returned root. Returning the empty
       tree keeps the tuple's nodes in memory while we retain their root hash. *)
    let* _ =
      Attributes.with_batch tx (fun () ->
          let* node = tree tx (fun blob -> Ok (Concepts.Hash.hash_of_blob blob)) tuple in
          root := node;
          Ok Attributes.empty )
    in
    Ok (Attributes.hash_of !root)

  let store tx tuple =
    let* node = Attributes.with_batch tx (fun () -> tree tx (SI.store_blob tx) tuple) in
    (* Persist explicitly so the empty tuple is stored as well. *)
    Attributes.persist tx node

  let node tx root =
    let* node = Attributes.find tx root in
    Option.to_result
      ~none:
        Concepts.Condition.(
          condition "invalid-tuple-root" "The tuple's attribute tree is missing from storage"
            ("hash" |=| Concepts.Value.String (Concepts.Hash.to_hum_string root)) )
      node

  let value_at tx address =
    SI.get_req tx (S.Hash address)
    |> fmap Concepts.Encoding.Bencode.of_blob
    |> fmap Concepts.Encoding.Value.value_of_bencode

  let load tx root =
    let* node = node tx root in
    Attributes.fold_left tx
      (fun acc name address ->
        let* acc = acc in
        let* value = value_at tx address in
        Ok (BatFingerTree.snoc acc (name, value)) )
      (Ok BatFingerTree.empty) node
    |> Result.join
    |> fmap Concepts.Tuple.of_bindings

  let names tx root = node tx root |> fmap (Attributes.keys tx)
  let attribute_address tx root name = node tx root |> fmap (Attributes.lookup tx name)

  let attribute tx root name =
    let* address = attribute_address tx root name in
    match address with
    | None -> Ok None
    | Some address -> value_at tx address |> Result.map Option.some
end
