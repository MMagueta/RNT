module type SCHEMA = sig
  type t
  type attribute
  val attributes : t -> (string * attribute) BatFingerTree.t
  val project : t -> string BatFingerTree.t -> (t, Concepts.Condition.condition) result
  val rename : t -> (string * string) BatFingerTree.t -> (t, Concepts.Condition.condition) result
  val merge : t -> t -> (t, Concepts.Condition.condition) result
end
