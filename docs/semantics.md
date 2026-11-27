# Constraint semantics and text format

## Node model

Variable nodes and abstract objects have separate zero-based ID spaces. A
points-to set contains **object IDs**, never variable-node IDs. Each object must
map to exactly one addressable variable node (`location`); that node represents
the pointer-valued storage associated with the object. This lets loads and
stores use ordinary copy edges without conflating objects and variables.

The text format is UTF-8, line-oriented, and versioned by the required first
non-comment line `pta-v1`. Blank lines and text after `#` are ignored. Records:

```text
vars N
objects M
location OBJECT_ID VARIABLE_NODE_ID
addr VARIABLE_NODE_ID OBJECT_ID
copy SOURCE_VARIABLE DESTINATION_VARIABLE
load POINTER_VARIABLE DESTINATION_VARIABLE
store POINTER_VARIABLE SOURCE_VARIABLE
```

`vars` and `objects` occur exactly once. Every object has exactly one `location`
record. Variable and object IDs are in `[0,N)` and `[0,M)` respectively.
Duplicate constraints are accepted and have no semantic effect. Unknown
records, malformed integers, missing mappings, and out-of-range IDs are errors.
Records may appear in any order after the version line.

## Least-fixed-point meaning

`addr(p,o)` adds `o` to `PT(p)`. `copy(q,p)` means `PT(q) ⊆ PT(p)`.
`load(q,p)` means each `o ∈ PT(q)` induces `location(o) → p`.
`store(p,q)` means each `o ∈ PT(p)` induces `q → location(o)`. Every induced
edge propagates the source's existing points-to set immediately, and all later
insertions propagate over it. The result is the least fixed point of these
monotone rules. No casts, offsets, or unknown objects are implicit in this
format.
