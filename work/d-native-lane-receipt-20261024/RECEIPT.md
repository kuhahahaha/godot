# D Godot native lane receipt — 2026-10-24

## Scope

This receipt covers the D-owned Godot native lane for K03, E15/E16, E25, and P01 successor boundaries. It is an acceptance handoff, not a claim that the full Godot build, GPU/runtime, PCK, platform, or performance matrix is qualified.

## Adopted source evidence

- K03 typed-close/deferred retirement: `546cfea0d3`, `d07dffb7c3`.
- E15/E16 owner-thread/device retirement follow-up: `6b321d7a0d`.
- E25 source account/provenance and revision lineage: `0c34eac282`, `28f46554cc`, `f66ee43f04`, `fd67eb0ef1`, `819dc48c4d`, `77e0a73bfc`.
- P01 unused receiver placeholder removal: `8cf518b407`.
- Existing D-side acceptance reconciliation: `3772028318`, `7222dc0a61`.

## Current status by boundary

| Boundary | Source state | Qualification state | Required next owner input |
|---|---|---|---|
| K03 | Typed close, recreate, cleanup, deferred-retirement gates are present and statically reviewed. | Focused source evidence only; no new runtime credit. | A build containing the selectors, then focused native fixture execution. |
| E15/E16 | Device identity, late-callback poison, physical busy retirement, per-resource acknowledgement, and error propagation are present in the reviewed owner code. | `acceptance=false`, `runtime_credit=false`, `native_qualification=false`, `gpu_qualification=false`. | Rebuilt test binary with E15/E16 selectors and an available build volume. |
| E25 | Provenance admission, source account fields, bounded mirrors, invalidation, and `UINT64_MAX -> 1` revision policy are present in the committed lineage. | Static/source evidence only; capture/runtime qualification remains pending. | Consumer/lease owner for source bytes and a runtime build. |
| P01 | Production `modules/p01_probe` and receiver placeholder are absent; successor test module is isolated under `modules/skinned_mesh_capture_tests`. | No production P01 receiver or runtime qualification is claimed. | Explicit owner consumer and build/runtime receipt if P01 is still required. |

## Hard blockers

1. Full Godot/editor test build is blocked by the recorded macOS archive failure `No space left on device` (`work/e25-scope-split-20261024/isobase-build.log`).
2. Existing binaries do not contain the new E15/E16 selectors; a zero-matched selector run is not runtime evidence.
3. E25 has no adopted consumer/lease owner in this lane; source mirrors cannot mint a capture lease or runtime qualification.
4. P01 has no adopted production receiver owner. The removed placeholder must not be recreated.

## Verification performed for this receipt

- Read-only source inspection of the named commits and current Godot sources.
- Confirmed no production `modules/p01_probe` directory or receiver placeholder remains.
- Confirmed `git diff --check` for the receipt-only change.
- No full build was repeated because the known disk-space blocker is unchanged.

## Explicit non-claims

This receipt does not mark any canonical acceptance row complete, and does not claim native, GPU, runtime, PCK, platform, performance, or production readiness. Further D work should resume immediately when the owner input or rebuilt binary is available; until then, repeating source-only defenses would be duplicate work.
