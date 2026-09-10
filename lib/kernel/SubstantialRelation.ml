module Make (S : Abstract.Storage.STORAGE) (Schematics : sig
  include Protocols.Schematics.S
  val of_blob : Concepts.Blob.t -> (Schema.t, Concepts.Condition.condition) result
end) = struct
  module SI = Storage.Make (S)

  module Error = struct
    open Concepts.Condition

    let malformed_relation () =
      condition "malformed-substantial-relation"
        "The on-disk representation of a substantial relation did not conform to what was \
         expected. Is your database corrupted?"
        empty

    let malformed_tuple_key length =
      condition "malformed-tuple-key" "A tuple key did not have the expected hash size"
        ("length" |=| Concepts.Value.Integer length)

    let invalid_tuple_root root =
      condition "invalid-tuple-root"
        "The tuple-set root for a substantial relation is missing from the backend. Either your \
         storage is corrupted, or this is a bug in RNT!"
        ("hash" |=| Concepts.Value.String (Concepts.Hash.to_hum_string root))
  end

  module TupleKey = struct
    type t = Concepts.Hash.hash

    let encode = Concepts.Hash.blob_of_hash

    let decode blob =
      let raw = Concepts.Blob.bytes_of_blob blob |> Bytes.to_string in
      if String.length raw = Concepts.Hash.size then Ok (Concepts.Hash.of_raw_string raw)
      else Error (Error.malformed_tuple_key (String.length raw))

    let compare = Concepts.Hash.compare
  end

  module TupleSet = Merkle.Make (S) (TupleKey)

  type t =
    { heading: Concepts.Hash.hash;
      predicate: Concepts.Hash.hash option;
      local_constraints: Concepts.Hash.hash option;
      tuples: TupleSet.address;
      indexes: Concepts.Hash.hash option }

  module rec Representation : (Concepts.Encoding.Record.S with type t = t) =
    Concepts.Encoding.Record.Make (Body)

  and Body : Concepts.Encoding.Record.BODY = struct
    type nonrec t = t

    let tag = 'R'
    let malformed = Error.malformed_relation

    let fields {heading; predicate; local_constraints; tuples; indexes} =
      let open Concepts.Encoding in
      [ "heading", Value.bencode_of_hash heading;
        "predicate", Value.bencode_of_option Value.bencode_of_hash predicate;
        "local-constraints", Value.bencode_of_option Value.bencode_of_hash local_constraints;
        "tuples", Value.bencode_of_hash tuples;
        "indexes", Value.bencode_of_option Value.bencode_of_hash indexes ]

    let of_fields fields =
      let open Utilities.Result in
      let open Concepts.Encoding in
      let* heading = Bencode.field "heading" fields |> fmap Value.hash_of_bencode in
      let* predicate =
        Bencode.field "predicate" fields |> fmap (Value.option_of_bencode Value.hash_of_bencode)
      in
      let* local_constraints =
        Bencode.field "local-constraints" fields
        |> fmap (Value.option_of_bencode Value.hash_of_bencode)
      in
      let* tuples = Bencode.field "tuples" fields |> fmap Value.hash_of_bencode in
      let* indexes =
        Bencode.field "indexes" fields |> fmap (Value.option_of_bencode Value.hash_of_bencode)
      in
      Ok {heading; predicate; local_constraints; tuples; indexes}
  end

  let encode = Representation.to_blob
  let decode = Representation.of_blob

  let heading {heading; _} = heading
  let predicate {predicate; _} = predicate
  let local_constraints {local_constraints; _} = local_constraints
  let tuples {tuples; _} = tuples
  let indexes {indexes; _} = indexes
  let hash relation = Representation.to_blob relation |> Concepts.Hash.hash_of_blob

  let tuple_node tx relation =
    let open Utilities.Result in
    let* tuples = TupleSet.find tx relation.tuples in
    Option.to_result ~none:(Error.invalid_tuple_root relation.tuples) tuples

  let empty tx ~heading ?predicate ?local_constraints ?indexes () =
    let open Utilities.Result in
    let* tuples = TupleSet.empty_under tx in
    Ok {heading; predicate; local_constraints; tuples; indexes}

  let store tx relation = SI.store_blob tx (Representation.to_blob relation)

  let contains_tuple tx relation tuple =
    let open Utilities.Result in
    let tuple = Concepts.Hash.hash_of_blob tuple in
    let* node = tuple_node tx relation in
    let* present = TupleSet.lookup tx tuple node in
    Ok (Option.is_some present)

  let heading_value tx relation =
    let open Utilities.Result in
    let* data = SI.get_req tx (S.Hash relation.heading) in
    Schematics.of_blob data

  let enumerate storage relation =
    let open Utilities.Result in
    let* tx = S.start storage in
    let* node = tuple_node tx relation in
    (* TODO: `TupleSet.keys` walks the whole Merkle structure eagerly,
       before the first tuple is yielded. Fine for this sketch; a real
       implementation should walk it lazily instead (its own small
       generator, or an explicit stack over Merkle.node) so that
       stacking e.g. `restrict` on a huge relation does not pay for the
       entire scan before the first `next`. *)
    let* keys = TupleSet.keys tx node in
    let remaining = ref keys in
    let closed = ref false in
    let close () = if not !closed then begin closed := true; ignore (S.abort tx) end in
    let produce ~yield =
      let rec loop () =
        match BatFingerTree.front !remaining with
        | None ->
           close ();
           Ok ()
        | Some (rest, addr) ->
           remaining := rest;
           match SI.get_req tx (S.Hash addr) |> fmap Concepts.Tuple.Representation.of_blob with
           | Error _ as e ->
              close ();
              e
           | Ok tuple ->
              yield tuple;
              loop ()
      in
      loop ()
    in
    Ok (Generator.cursor_of ~on_release:close produce)

  class relation storage value =
    object (self)
      inherit Lifecycle.null
      val storage : S.connection = storage
      val relation : t = value
      method heading = Ok relation.heading
      method predicate = Ok relation.predicate
      method local_constraints = Ok relation.local_constraints

      (* The protocol takes a tuple, not bytes: encoding is this object's business, and a caller
         that had to produce the exact stored bytes would have to know this encoding to do it. *)
      method contains (tuple : Concepts.Tuple.t) =
        SI.with_transaction storage (fun tx ->
            contains_tuple tx relation (Concepts.Tuple.Representation.to_blob tuple) )

      method schema = SI.with_transaction storage (fun tx -> heading_value tx relation)

      (* A substantial relation is finitely enumerable, so it carries [Enumerable] as well as
         [Relation]. A procedural relation would carry only the latter, which is how an evaluator
         discovers it cannot iterate one -- see [Protocols.Enumerable]. The context is accepted and
         unused here: this enumeration reads through its own cursor-lifetime transaction and needs
         no name resolution, but cancellation should eventually be checked between tuples. *)
      method enumerate (_ : Protocols.Context.t) = enumerate storage relation

      method protocols : Protocols.Handle.protocol list =
        [Protocols.Relation.make self; Protocols.Enumerable.make self; Schematics.make self]
      method hash = hash relation
    end

  let make conn ~heading ?predicate ?local_constraints ?indexes () =
    let open Utilities.Result in
    SI.with_transaction conn (fun tx ->
        let* relation = empty tx ~heading ?predicate ?local_constraints ?indexes () in
        let* _ = store tx relation in
        Ok (new relation conn relation |> Protocols.Handle.make) )

  let load_value tx addr =
    let open Utilities.Result in
    let* data = SI.get_req tx (S.Hash addr) in
    Representation.of_blob data

  let wrap conn relation =
    new relation conn relation |> Protocols.Handle.make |> Result.ok

  let load tx conn addr =
    load_value tx addr |> Utilities.Result.fmap (wrap conn)

  let assert_tuple tx relation tuple =
    let open Utilities.Result in
    let* tuple = SI.store_blob tx tuple in
    let* node = tuple_node tx relation in
    let* node = TupleSet.insert tx tuple tuple node in
    Ok {relation with tuples= TupleSet.hash_of node}
end
