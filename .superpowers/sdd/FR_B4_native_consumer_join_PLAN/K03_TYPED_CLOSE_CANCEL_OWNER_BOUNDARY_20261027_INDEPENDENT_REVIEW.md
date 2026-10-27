# Independent review — K03 typed-close cancel owner boundary — 2026-10-27

- Reviewer: `cancel_owner_boundary_review` (non-author)
- Source basis: `d/b4-native-output-20261027`, commit `1753faf666`
- Result: **limited-scope accept**, no actionable P0/P1/P2.
- Tests: not rerun; source census only.

The reviewer confirmed that ClassDB binds the cancel method on `RenderingServer`
and `RenderingServerDefault` only forwards it on the render thread. The RD
`ParticlesStorage` implementation returns `ERR_INVALID_PARAMETER` for cancel
without mutating a row, while the wrapper may return `ERR_UNAVAILABLE` before
the call off-thread. Acquire first checks object/withdrawal state and then
returns `native_output_source_unavailable`; seal returns
`invalid_external_lease` without recording output.

`ExternalOutputStorage::release_native_owner()` performs the final
`can_destroy()` gate and authenticated Registry metadata release. The reviewer
confirmed this is not a cancel primitive and that no lease/ticket/operation
caller reaches the cancel path. The minimum owner inputs—same authenticated
ticket, same-request association, checked cancel disposition, and retirement
receipt—are precise and preserve G1.

No Released, completion, readback, Ready/Verified, packet, physical-disposal,
or GPU qualification claim is made.
