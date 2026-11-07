# Native depth allocation R2 final evidence — 2026-11-07

R2 final host SHA-256: `c5e4bc1951643a6968c69a73d15b19abd0479de174bfa8e5898a4a689a83f939`.
The original successful build, full CPU XML/log, GPU runner report and both cases' stdout/stderr are preserved under `parent/`. Full CPU: 2269 cases, 585464 assertions, zero failures, three skipped. Real Metal Mobile GPU: two cases, 29 successful checks each. Ten hooks passed in `FINAL_HOOKS_R3.json`.

`final_source/` preserves all 19 exact R2 sources against `parent/FINAL_R2_SOURCE_PINS.json`. Snapshots were fixed before subsequent height work changed the working sources. The README pin was refreshed by the parent to match the bytes already pinned in the R2 GPU report. Source identity captured at runtime is not itself build provenance. The build/host records are retained separately. No engine binary is included.

The R2 policy adds `framebuffer_owner_reservation`: Known 0 is admission credit because this framebuffer has no separate MetalHeap backing reservation. It does not measure physical framebuffer cost as zero bytes. Driver framebuffer backing, framebuffer object, attachment reference and opaque driver overhead remain UnknownNotExposed. Texture owner reservation is independent: 16640 bytes for the small pool case and 135266304 bytes for the large dedicated case; observed depth backing is respectively 16512 and 135266304 bytes.

`independent_review/` is the original independent review of the preceding R1 revision. It is not a new independent approval of R2 framebuffer credit. `R1_TO_R2_SOURCE_DIFFERENCES.json` and `r1_to_r2_diff/` provide exact source differences, including the additional CPU assertions, runner serialization and explicit README credit policy. CPU assertion count increased from R1's 585450 to 585464.

`historical_neighbor/` contains the completed original viewport, legacy and neighbor results/logs and eight viewport PNGs. These belong to historical host `7e2c5796c813a48b7587fbec6356c0ae9f90f735abf621cf0c6ea8e17b317774`, not the R2 host. They must not be represented as rerun on c5e4bc…. The original R1 archive remains unchanged.

`parent/RED_R4_CONTROLLED_MUTATION_RECEIPT.json` preserves the parent's controlled command-history receipt. The negative source hash was reconstructed later from the preserved preimage and exact controlled mutation, not independently measured contemporaneously at R4. R4 runtime source pins captured restored green source, not mutant build provenance. The old R1 archive retains the original failing R4 host/results/logs. Old CPU aborts and build failures are not the target RED evidence.

These private helpers and fixtures do not establish production cross-DSO Scene/C or HeightFrame producer completion, whole-output release or reservation refund.

Run `python3 verify_archive.py` here for offline stored/decompressed hashes, 19 source pins, CPU XML, GPU checks/credit/physical-unknown fields, hook results, historical hosts, RED reconstruction and unchanged historical R1 manifest entries. No current source files or engine are read. The old R1 verifier's exact directory inventory predates this appended `r2/` directory; use this verifier to validate both scopes without modifying old evidence.
