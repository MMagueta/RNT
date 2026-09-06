module Error = struct
  open Condition

  let duplicate_attribute name =
    condition "duplicate-attribute" "An attribute name occurs more than once in a tuple"
      ("attribute" |=| Value.String name)
end

type t = (string, Value.value) BatMap.t

let empty = BatMap.empty

let of_list bindings =
  let open Utilities.Result in
  List.fold_left
    (fun tuple (name, value) ->
      let* tuple = tuple in
      if BatMap.mem name tuple then Error (Error.duplicate_attribute name)
      else Ok (BatMap.add name value tuple) )
    (Ok empty) bindings

let to_list tuple = BatMap.bindings tuple
let names tuple = BatMap.keys tuple |> BatList.of_enum
let find tuple name = BatMap.find_opt name tuple
let mem tuple name = BatMap.mem name tuple
let degree tuple = BatMap.cardinal tuple
let equal = BatMap.equal Value.equal
