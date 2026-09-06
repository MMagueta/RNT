type t = {state: Concepts.Hash.hash}

let make ~state = {state}
let state {state} = state
