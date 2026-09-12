type t = string list

let this = []
let (@/) x y = x::y

let to_string path =
  path
  |> List.map (fun x ->
         let str = BatString.replace ~str:x ~sub:"^" ~by:"^^" |> snd in
         BatString.replace ~str ~sub:"/" ~by:"^/" |> snd)
  |> BatIO.to_string (BatList.print ~first:"/" ~last:"" ~sep:"/" BatString.print)

module Error = struct
  open Concepts.Condition

  let path_not_found path = condition "path-not-found" "A given path was not found under the specified object"
                              ("path" |=| Concepts.Value.String (to_string path))

  let not_a_registry path = condition "not-a-registry" "The specified path did not point to a registry"
                               ("path" |=| Concepts.Value.String (to_string path))
end

let rec lookup handle = function
  | [] -> Ok (Some handle)
  | x::xs ->
     let open Utilities.Result in
     let open Protocols in
     match Protocols.Directory.from handle with
     | None -> Ok None
     | Some dir ->
        let* elem = Directory.find dir x in
        match elem with
        | None -> Ok None
        | Some elem -> lookup elem xs

let lookup' handle path = lookup handle path
                          |> Result.map (Option.to_result ~none:(Error.path_not_found path))
                          |> Result.join

let update handle path key reference value =
  let open Utilities.Result in
  let* handle = lookup' handle path in
  let* registry = Protocols.Registry.from handle |> Option.to_result ~none:(Error.not_a_registry path) in
  Protocols.Registry.update registry key reference value
