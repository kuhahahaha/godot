# Independent review — E20 graph handle/path slice

The change is limited to existing `RenderingDeviceGraph` record storage and
does not create a parallel graph or resource authority.

- Texture clear/update handles use the same graph instance, frame, command
  index and kind-1 identity already used by buffer commands.
- `contains_record` validates the recorded command type before accepting a
  handle; stale/foreign frames remain rejected.
- `has_dependency_path` resolves both handles through `contains_record`, walks
  only `command_list_nodes`, bounds every index by `command_count`/vector size,
  rejects backward edges, and returns a measured edge count.
- Existing callers ignore the new return values, preserving current behavior.
- Red/green build evidence and focused/native regression counts are present;
  no release/UBSan or GPU/Ready claim is present.

The separate release probe is unavailable before compilation because the
existing identity parser rejects an unclosed `-S` linker operand (rc2). This is
an existing linker-branch boundary, not a graph implementation failure.

Disposition: no P1/P2 finding in this bounded review. The slice is a real
native prerequisite for the later typed transfer contract, which remains
unimplemented and owner-gated.
