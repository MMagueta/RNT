type t = string BatFingerTree.t

let this = BatFingerTree.empty

let of_string s =
  String.split_on_char '/' s |> List.filter (fun s -> s <> "") |> BatFingerTree.of_list

let (@/) segment path = BatFingerTree.cons path segment

let rec lookup handle path =
  let open Utilities.Result in
  match BatFingerTree.front path with
  | None -> Ok (Some handle)
  | Some (rest, segment) -> (
     match Protocols.Directory.from handle with
     | None -> Ok None
     | Some dir -> (
        let* found = Protocols.Directory.find dir segment in
        match found with
        | None -> Ok None
        | Some handle' -> lookup handle' rest))
