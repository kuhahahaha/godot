# E20 qualification graph-path consumer — 2026-10-29

This native slice connects the existing qualification sealing reducer to the
real graph-path API landed in the preceding graph slice. It replaces direct
single-edge checks with the measured adjacency path predicate for capture→body
and prepared semantic-6 source→capture relations. No native completion,
readback, Ready or G1 authority is added.

## TDD evidence

- New test creates three actual graph commands over one resource. The first and
  third have no direct synchronization edge, but the graph reports a real path
  length of 2.
- RED build/test: compile rc0, focused test rc1 with
  `ticket.ordering_closed == false` and `ticket.mapping_failed == true` under
  the old direct-edge consumer.
- GREEN build rc0 and focused transitive-path test rc0: 1 case / 5 assertions.
- Graph regression: 8 cases / 109 assertions rc0.
- Full qualification tag regression: 20 cases / 344 assertions rc0.
- `git diff --check` on the production/test files: rc0.

The path result is used only as an ordering fact during sealing. Native issue,
completion, exact readback, physical retirement and Ready/G1 gates remain
unchanged and unclaimed.
