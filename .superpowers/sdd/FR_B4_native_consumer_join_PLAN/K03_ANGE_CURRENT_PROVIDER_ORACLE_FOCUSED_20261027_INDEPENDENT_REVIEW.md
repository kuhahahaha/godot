# ANGE current provider/oracle focused independent review — 2026-10-27

Read-only review of the provider/oracle report, receipt, review inputs, and
three raw reports; no source edits or reruns. All three report hashes match.

Mutation/oracle evidence is consistent: curve baseline/restored 2,108
assertions with 11 mutants rejected; mesh 555 with 14 mutants rejected and E23
neighbor 1,953; texture 5,242 with 13 mutants rejected. Oracle portability
passes its isolated unittest.

The texture runner maintenance gap is recorded separately: stale expectation
5,230 versus observed 5,242, with the missing `capture_shape_plan_and_ranges`
section contributing 12 assertions. A temporary harness correction ran green;
the tracked/untracked runner was not edited. This does not invalidate the
provider evidence or create runtime authority.

Scope is private CPU pure-provider semantics and mutation/oracle checks only.
No owner, capture transaction, native qualification, operation/completion/
readback, Ready, Verified, or GPU authority is established; all claims are
`false`.

**Decision: limited-scope accept; no P0–P3 findings.**
