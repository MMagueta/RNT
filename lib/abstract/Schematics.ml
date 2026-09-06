(** Schema operations to be supplied. *)
module type SCHEMA = sig
  type t

  (** Attribute metadata supplied by the logical model. In this
      initial slice, types are opaque tags carried with the
      attribute. They are neither resolved nor checked against values
      or other type tags. *)
  type attribute

  (** Named attributes, with no duplicate names. List order has no
      logical significance. Inspecting a schema must not enumerate
      relation tuples. *)
  val attributes : t -> (string * attribute) list

  (** Select existing attributes, preserving their metadata and
      origins.  Unknown or duplicate names produce
      conditions. Selecting no attributes is valid. These are
      schema-shape checks, not type checks. *)
  val project : t -> string list -> (t, Concepts.Condition.condition) result

  (** Combine schemas for tuple composition. The schema implementation
      handles shared names and their metadata. Type compatibility and
      domain membership checks are outside this initial interface. *)
  val merge : t -> t -> (t, Concepts.Condition.condition) result
end
