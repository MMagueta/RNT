(** Store tuples as Merkle trees that map attribute names to value addresses.
    The root identifies the tuple, and equal values share one stored copy. *)
module Make (S : Abstract.Storage.STORAGE) : sig
  (** The address of a tuple's tree root. *)
  type address = Concepts.Hash.hash

  (** Compute a tuple's address without storage access.
      Equal tuples have the same address, regardless of attribute order. *)
  val address_of : Concepts.Tuple.t -> address

  (** Store a tuple and return its address, matching [address_of].
      Values already stored are shared. *)
  val store : S.transaction -> Concepts.Tuple.t -> (address, Concepts.Condition.condition) result

  (** Read the whole tuple. Use [attribute] to read individual values. *)
  val load : S.transaction -> address -> (Concepts.Tuple.t, Concepts.Condition.condition) result

  (** List attribute names in sorted order without loading their values. *)
  val names : S.transaction -> address -> (string list, Concepts.Condition.condition) result

  (** Read one attribute, returning [None] if its name is absent.
      Missing or malformed stored data returns an error. *)
  val attribute :
    S.transaction ->
    address ->
    string ->
    (Concepts.Value.value option, Concepts.Condition.condition) result

  (** Look up an attribute's storage address without loading its value. *)
  val attribute_address :
    S.transaction ->
    address ->
    string ->
    (Concepts.Hash.hash option, Concepts.Condition.condition) result

  (** Store a tuple containing only the selected attributes, reusing their values.
      Unknown or duplicate names return an error. An empty list gives the empty tuple. *)
  val project :
    S.transaction -> address -> string list -> (address, Concepts.Condition.condition) result
end
