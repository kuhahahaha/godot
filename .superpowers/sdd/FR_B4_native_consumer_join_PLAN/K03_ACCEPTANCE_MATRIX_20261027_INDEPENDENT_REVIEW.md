# K03 acceptance matrix independent review — 2026-10-27

Read-only review of the refreshed eight-package matrix and its JSON/receipt/
review-input artifacts. No source edits, builds, or fixture reruns were made.

All eight package prefixes resolve in the persistent checkout: `1921759a6e`,
`2008230c5f`, `82604358c4`, `e8f4781870`, `8b3b0af0ae`, `409527e584`,
`fc03fd5c18`, and `1d683b0c61`. The added withdraw/retire audit scope is
consistent: withdrawal tombstone and Busy/Withdrawing admission, NativeUse
retirement, authenticated Registry release, and destructor ordering are
covered, while public physical-disposal/Released receipts and producer
association remain outside scope.

Pair identity remains correctly layered: historical fixed pair, prior ABI-
negative pair (rc2 before world entry due to missing
`device_exit_register_scene_ready_v2`), and current aligned editor/ANGE/DSO
pair with fresh non-headless Metal/mobile probes are distinct. Withdrawal and
Registry release evidence is not conflated with public physical disposal or
Released claims.

The global matrix conservatively leaves U08, E09, operation, completion,
readback authority/bytes, packet publication, physical disposal, Released,
Ready, Verified, and GPU all `false`.

**Decision: limited-scope accept; no P0–P3 findings.**
