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

## Path-length follow-up

The ticket now retains the measured capture→body path length internally as
`ordering_path_length`, and each graph member retains its measured source path
length. RED first failed to compile because the field was absent; GREEN build
and focused test pass with the transitive length equal to `2`. The follow-up
Graph regression remains 8/109 and Qualification is 20/345, rc0.

## Prepared-source path coverage

The focused case now also carries a prepared semantic-6 source member. The
source→capture path is retained as `GraphMember::path_length == 1`, while the
capture→body path remains `ordering_path_length == 2`. The incremental build
and focused run are rc0 with 1 case / 7 assertions; the full Qualification
regression is 20/346 and the Graph regression is 8/109, both rc0. This adds
coverage for the internal ordering fact only and does not add completion,
readback, Ready or G1 authority.
