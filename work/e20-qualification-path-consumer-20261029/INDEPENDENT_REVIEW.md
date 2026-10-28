# Independent review — qualification path consumer

- The reducer now uses `has_dependency_path`, which validates both handles and
  traverses actual graph adjacency. It does not convert a valid handle, frame
  order or synchronization request into a fabricated edge.
- The test proves a two-edge path with no direct capture→body edge and fails
  before the production change, then passes after it.
- Prepared source predecessors retain the existing earlier-frame branch and
  completion checks; only same-graph relation evaluation uses the path query.
- Existing focused graph and qualification regressions remain green.

The follow-up test carries a prepared semantic-6 predecessor and observes
`GraphMember::path_length == 1` while `ordering_path_length == 2`; the focused
case is 1/7 and the full Qualification regression is 20/346. The same source
also builds as macOS arm64 `template_release` with rc0 and passes `--version`.
These are bounded ordering/build checks and do not establish owner authority,
completion, readback, Ready or G1.

Disposition: no P1/P2 finding. This closes an ordering-consumer gap only; the
full typed texture owner/transfer and native completion/readback chain remains
open.
