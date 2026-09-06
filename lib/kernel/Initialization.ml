module Make (S : Abstract.Storage.STORAGE) = struct
  module BM = BranchManager.Make (S)

  (* Create and return a child namespace under [parent]. *)
  let namespace_under parent name =
    let open Utilities.Result in
    let child = Namespace.make () in
    let registry = Protocols.Registry.from parent |> Option.get in
    let* _ = Protocols.Registry.update registry name None (Some child) in
    Ok child

  let initialize conn =
    let open Utilities.Result in
    let open Protocols in
    let root = Namespace.make () in
    let registry = Registry.from root |> Option.get in
    let* branch_manager = BM.make conn "rnt-head" in
    let* _ = Registry.update registry "branch" None (Some branch_manager) in
    let* system = namespace_under root "system" in
    (* Clients register their evaluators under /system/evaluator. *)
    let* _ = namespace_under system "evaluator" in
    Ok root
end
