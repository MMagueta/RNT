module Make (S : Abstract.Storage.STORAGE) = struct
  module SI = Storage.Make (S)

  type t = {
    heading: Concepts.Hash.hash;
    evaluator: string;
    source: string;
    entry: string;
    bindings: Concepts.Hash.hash;
    inputs: string BatFingerTree.t
  }

  module Error = struct
    open Concepts.Condition

    let malformed () =
      condition "malformed-ephemeral-relation"
        "The on-disk representation of an ephemeral relation did not conform to what was expected"
        empty

    let replay_not_implemented () =
      condition "ephemeral-relation-replay-not-implemented"
        "Replaying an ephemeral relation's stored plan is not implemented in this slice -- \
         see lib/kernel/EphemeralRelation.mli"
        empty
  end

  let heading {heading; _} = heading
  let evaluator {evaluator; _} = evaluator
  let inputs {inputs; _} = inputs

  let make ~heading ~evaluator ~source ~entry ~bindings ~inputs () =
    Ok {heading; evaluator; source; entry; bindings; inputs}

  module rec Representation : (Concepts.Encoding.Record.S with type t = t) =
    Concepts.Encoding.Record.Make (Body)

  and Body : Concepts.Encoding.Record.BODY = struct
    type nonrec t = t

    let tag = 'E'
    let malformed = Error.malformed

    let fields {heading; evaluator; source; entry; bindings; inputs} =
      let open Concepts.Encoding in
      [ "heading", Value.bencode_of_hash heading;
        "evaluator", Field.string evaluator;
        "source", Field.string source;
        "entry", Field.string entry;
        "bindings", Value.bencode_of_hash bindings;
        "inputs", Bencode.List (BatFingerTree.to_list inputs |> List.map Field.string) ]

    (* TODO: symmetric to `fields` above -- decode each field back with
       Concepts.Encoding.Bencode.{field,as_string,as_list} and
       Concepts.Encoding.Value.hash_of_bencode, then rebuild the
       BatFingerTree for `inputs` with BatFingerTree.of_list. Left
       unimplemented because nothing in this slice ever calls `load`
       on a real address (see `load` below); the algorithm is exactly
       SubstantialRelation.Body.of_fields's shape, one field wider. *)
    let of_fields _fields = Error (malformed ()) [@@warning "-27"]
  end

  let store tx relation = SI.store_blob tx (Representation.to_blob relation)

  (* TODO: `enumerate`/`contains` on the Relation object this should
     return must, on every call:
     1. resolve `evaluator` via the *current* Runtime.Context's root,
        not the one live when `make` ran (lifecycle.org: "not
        version-pinned paths" -- the whole point of storing a path
        instead of a hash for `evaluator` and `inputs`);
     2. `Protocols.Evaluator.admit source` against it;
     3. `Protocols.Program.invoke ~entry ~bindings:(decode bindings)`;
     4. drain the resulting Cursor into this call's answer.
     None of that belongs in this module (see the module comment on
     layering) -- it belongs in whatever session/host module builds
     Runtime.Context.t values, since only that code is already
     S-aware AND Protocols-aware. `load` therefore returns a handle
     whose Relation methods all fail with `Error.replay_not_implemented`
     rather than wiring a half-generic version of that four-step
     replay here. *)
  let load tx conn addr =
    let open Utilities.Result in
    let* data = SI.get_req tx (S.Hash addr) in
    let* (_relation : t) = Representation.of_blob data in
    let _ = conn in
    Error (Error.replay_not_implemented ())
end
