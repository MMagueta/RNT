type t

class type implementation = object
  (** Labels clients can use to choose an evaluator, such as [drl] or [dml].
      Their meaning is up to the evaluator and its clients. *)
  method capabilities : string BatSet.t

  (** Validate a program in the evaluator's format and return its handle.
      The evaluator decides which checks to run and returns an error if they fail. *)
  method admit : string -> (Handle.t, Concepts.Condition.condition) result
end

val make : #implementation -> Handle.protocol
val from : Handle.t -> t Handle.interface option
val capabilities : t Handle.interface -> string BatSet.t

(** Check whether the evaluator declares this capability. *)
val declares : t Handle.interface -> string -> bool

val admit : t Handle.interface -> string -> (Handle.t, Concepts.Condition.condition) result
