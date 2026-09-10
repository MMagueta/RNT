type t = (string, string) BatMap.t

let of_alist pairs = List.fold_left (fun acc (name, domain) -> BatMap.add name domain acc) BatMap.empty pairs

(* Relies on BatMap's iteration order being key order (it is, for the
   polymorphic tree map keyed by Stdlib.compare on strings), rather
   than sorting again on every call. *)
let attributes heading =
  BatMap.foldi (fun name domain acc -> BatFingerTree.snoc acc (name, domain)) heading BatFingerTree.empty

let names heading = BatMap.foldi (fun name _ acc -> BatSet.add name acc) heading BatSet.empty
let arity heading = BatMap.cardinal heading
let mem name heading = BatMap.mem name heading

module Error = struct
  open Condition

  let unknown_attribute name =
    condition "unknown-attribute" "No such attribute on this heading"
      ("attribute" |=| Value.String name)

  let incompatible_domain name =
    condition "incompatible-domain" "The same attribute name is declared on two different domains"
      ("attribute" |=| Value.String name)
end

let domain_of name heading =
  match BatMap.find_opt name heading with
  | Some domain -> Ok domain
  | None -> Error (Error.unknown_attribute name)

let project names heading =
  let open Utilities.Result in
  BatFingerTree.fold_left
    (fun acc name ->
      let* acc = acc in
      let* domain = domain_of name heading in
      if BatMap.mem name acc then
        Error (Condition.condition "duplicate-attribute" "An attribute was selected more than once"
          Condition.("attribute" |=| Value.String name))
      else Ok (BatMap.add name domain acc))
    (Ok BatMap.empty) names

let rename pairs heading =
  let open Utilities.Result in
  BatFingerTree.fold_left
    (fun acc (old_name, new_name) ->
      let* acc = acc in
      let* domain = domain_of old_name heading in
      if BatMap.mem new_name acc then
        Error (Condition.condition "duplicate-attribute" "Two attributes have the same renamed name"
          Condition.("attribute" |=| Value.String new_name))
      else Ok (BatMap.add new_name domain acc))
    (Ok BatMap.empty) pairs

let shared left right = BatSet.intersect (names left) (names right)

(* TODO: this checks that shared attributes agree on domain, then
   takes their union favouring [right]'s bindings for the shared names
   (they already agree, so it doesn't matter which side wins). It does
   NOT guard against two attributes that happen to share a name and a
   domain tag by coincidence rather than by being "the same" join
   column. Domain tags are (for now at least) opaque unchecked
   strings. Refer to the docs/object-taxonomy.org Schematics section
   for more. *)
let joined left right =
  let open Utilities.Result in
  BatSet.fold
    (fun name acc ->
      let* acc = acc in
      let* dl = domain_of name left in
      let* dr = domain_of name right in
      if dl = dr then Ok acc else Error (Error.incompatible_domain name))
    (shared left right) (Ok ())
  |> Result.map (fun () -> BatMap.foldi BatMap.add right left)

let compatible left right = BatMap.equal ( = ) left right

module rec Representation : (Encoding.Record.S with type t = t) = Encoding.Record.Make (Body)

and Body : Encoding.Record.BODY = struct
  type nonrec t = t

  let tag = 'H'

  let malformed () =
    Condition.condition "malformed-heading"
      "The on-disk representation of a heading did not conform to what was expected" Condition.empty

  let fields heading =
    BatMap.foldi (fun name domain acc -> (name, Encoding.Bencode.String domain) :: acc) heading []

  let of_fields data =
    let open Utilities.Result in
    let* kvs = Encoding.Bencode.as_dict data in
    kvs
    |> List.map (fun (name, v) -> Encoding.Bencode.as_string v |> Result.map (fun d -> (name, d)))
    |> Utilities.List.sequence
    |> Result.map (List.fold_left (fun acc (name, d) -> BatMap.add name d acc) BatMap.empty)
end
