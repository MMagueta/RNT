open Rnt

module Make (S : Abstract.Storage.STORAGE) (C : Helpers.Storage.CONFIGURATOR) = struct
  open Alcotest
  module H = Helpers.Storage.Make (S) (C)
  module SR = Rnt.Kernel.SubstantialRelation.Make (S)
  module KT = Rnt.Kernel.Tuple.Make (S)

  let hash s = Concepts.Blob.blob_of_bytes (Bytes.of_string s) |> Concepts.Hash.hash_of_blob
  let tuple pairs = Concepts.Tuple.of_list pairs |> Helpers.condition_as_failure
  let text s = Concepts.Value.String s
  let alaric = tuple ["name", text "Alaric"; "city", text "Braga"]
  let brennus = tuple ["name", text "Brennus"; "city", text "Braga"]

  let assert_and_load conn =
    let original_contains, updated_contains, loaded_contains, other_contains, reordered_contains =
      begin
        let open Utilities.Result in
        let* tx = S.start conn in
        let* original = SR.empty tx ~heading:(hash "heading") () in
        let* original_contains = SR.contains_tuple tx original alaric in
        let* updated = SR.assert_tuple tx original alaric in
        let* updated_contains = SR.contains_tuple tx updated alaric in
        (* Membership is by the tuple's address, so a tuple that was
           never asserted is absent however much it shares. *)
        let* other_contains = SR.contains_tuple tx updated brennus in
        let* stored = SR.store tx updated in
        let* () = S.commit tx in
        let* tx = S.start conn in
        let* loaded = SR.load_value tx stored in
        let* loaded_contains = SR.contains_tuple tx loaded alaric in
        let* reordered_contains =
          SR.contains_tuple tx loaded (tuple ["city", text "Braga"; "name", text "Alaric"])
        in
        let* () = S.abort tx in
        Ok (original_contains, updated_contains, loaded_contains, other_contains, reordered_contains)
      end
      |> Helpers.condition_as_failure
    in
    check bool "original" false original_contains;
    check bool "updated" true updated_contains;
    check bool "loaded" true loaded_contains;
    check bool "a tuple sharing a value is not thereby a member" false other_contains;
    check bool "order of construction is not significant" true reordered_contains

  (* Asserting a tuple stores it where the kernel addresses it, so the
     relation and the tuple layer agree on what was asserted. *)
  let asserted_tuples_are_readable conn =
    begin
      let open Utilities.Result in
      let* tx = S.start conn in
      let* relation = SR.empty tx ~heading:(hash "heading") () in
      let* relation = SR.assert_tuple tx relation alaric in
      let* alaric_address = KT.address_of tx alaric in
      let* loaded = KT.load tx alaric_address in
      check bool "the asserted tuple reads back" true (Concepts.Tuple.equal alaric loaded);
      let* city = KT.attribute tx alaric_address "city" in
      check (option Helpers.value) "one attribute at a time" (Some (text "Braga")) city;
      let* relation = SR.assert_tuple tx relation brennus in
      let* brennus_address = KT.address_of tx brennus in
      let* one = KT.attribute_address tx alaric_address "city" in
      let* other = KT.attribute_address tx brennus_address "city" in
      check bool "asserted tuples share the values they agree on" true
        (Option.equal Concepts.Hash.hash_equals one other);
      let* both = SR.contains_tuple tx relation brennus in
      check bool "and both are members" true both;
      S.abort tx
    end
    |> Helpers.condition_as_failure

  let suite prefix =
    ( "substantial-relation/" ^ prefix,
      [ test_case "assert-and-load" `Quick
          (H.with_connection assert_and_load "substantial-relation-test");
        test_case "asserted-tuples" `Quick
          (H.with_connection asserted_tuples_are_readable "substantial-relation-test") ] )
end

module LMDB = Make (Rnt.Backend.Storage.LMDB) (Helpers.Storage.LMDB_Configurator)

let suites () = [LMDB.suite "lmdb"]
