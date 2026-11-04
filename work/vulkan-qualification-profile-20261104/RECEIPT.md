# Checked Vulkan qualification production package — 2026-11-04

Production commit: `0b43072494` (`fix: admit Vulkan qualification with checked native submission bound`). The Vulkan backend now supplies its checked original submission bound, allowing valid qualification requests through the unchanged graph-plus-native 4096-fact gate. No new scheduler, registry, authority source or account size was introduced.

## Production change and source proof

Vulkan emits one `vkQueueSubmit` per nonempty main/worker queue slot, combining all command buffers, waits and signals in one submit. Segment preparation adds no hidden queue call. The backend returns the original slot count up to 4096; the inherited profile checks frame validity and main/worker addition overflow. The original Registry gate additionally charges graph facts. `BOUND_AND_RETENTION.md` traces main/worker/frame paths, original source observation, observer/waiter retention, serial exhaustion, physical return and preserved failure policy.

The production delta is one header plus two native test files. Baseline tests had two independent compile errors, repaired separately as `8404d12720`: a missing callable include and a test-only CurveXYZTexture friend declaration. Neither changes runtime behavior.

## Source-matched build and ABI intake

The isolated worktree is `/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/worktrees/vulkan-qualification-profile-20261104`, branch `codex/vulkan-qualification-profile-20261104`. Build command `scons -Q tests=yes platform=macos target=editor -j4` exited 0. `build.log` retains the complete build, including the existing MoltenVK dependency's minimum-macOS linker warning; execution here was on the current Apple M3 Pro host. `baseline-build-failure.log` and `prerequisite-compile.log` retain the initial errors and successful focused repair compilation.

| Artifact | SHA-256 |
| --- | --- |
| `bin/godot.macos.editor.arm64` | `b173b2b13e82b9919bd6c0154258c6900e532c018bc3b58de70d2fad81b8f1b3` |
| Adjacent controlled-build metadata | `9bbddbd136412337946b2fc977bf1f5a12f095332d20341916787dad022a23ee` |
| Selected interface JSON | `ff15055d80992295d546b88e4a77d8f6741c6e1f9f06a6023a05fea890a8f784` |
| Frozen generated SDK `sdk/gdextension_interface.gen.h` | `d5ed173d160be54958031efb9c4e18a7e4d4806dd0d1e40fd33002cfe6d637ce` |

`CoreBuildSelection.schema1.json` is the closed selected identity for consumer intake; `controlled_build.json` preserves the actual metadata. The core/compiler stamps are `3a0dec39...` / `cc306155...`. The binary's version label names prerequisite HEAD `8404d1272` because it was linked with the production delta still uncommitted; `SOURCE_MANIFEST.json` pins the ten exact source/test files, whose bytes match production commit `0b43072494`.

An immutable local copy is retained at `work/vulkan-qualification-profile-20261104/host/editor.arm64`, with adjacent exact controlled-build metadata. Its bytes match the tested artifact, and its version invocation exited 0. That copy is available for later intake without overwriting it during the next renderer build; the large executable is retained locally rather than committed to Git.

This clean component baseline is based on `c3f59ef3aa`; its ABI does **not** include the separate, uncommitted System scalar additions from the shared main tree. It is a matched V1/typed-V2 qualification host and SDK, not a drop-in replacement for the current System product pair. A/B must select this exact component ABI when consuming it, or integrate the production commit into their adopted System-capable Godot source and rebuild a matching final pair. No existing main-tree binary or selected product pair was overwritten.

## Verification

| Gate | Result |
| --- | --- |
| Exact header-body arithmetic RED | Four required positive/profile rows failed with inherited bound zero; source-only arithmetic evidence |
| Same arithmetic GREEN | All extracted-source rows passed; engine proof is separate below |
| New Vulkan profile and original Registry capacity tests | 2/2 cases, 25/25 assertions, exit 0 |
| Vulkan completion/native-source regressions | 12/12 cases, 373/373 assertions, exit 0 |
| Metal completion regressions | 6/6 cases, 402/402 assertions, exit 0 |
| Qualification suite | 208/208 cases, 44,014/44,014 assertions, exit 0 |
| Actual Vulkan main-RD typed/V1 processes | 6/6 exited 0, 54/54 runner checks |
| Actual Metal main-RD typed/V1 processes | 6/6 exited 0, 54/54 runner checks |

`native-tests/results.json` retains exact commands, process return codes, counts and raw-log hashes. Those selections overlap and are not added into a unique case total. The qualification suite deliberately exercises refusal paths and emits their diagnostics; none were relabeled as live GPU acceptance. The separate visible driver runs require actual backend/device startup lines and reject engine errors.

The existing native runner was reused with a driver option, backend-bound container-generation metadata and exact startup-header checks. Final actual runs are `runtime/results/20261104-085722-vulkan` and `runtime/results/20261104-085828-metal`. Each includes native generation, typed positive, typed accepted-prefix cancellation, V1 generation, V1 ticket and V1 prepared upload/record. Their retained-input snapshots pin source, header, DSOs, GLSL, generated containers and generation metadata; all 46 artifact hashes/sizes per run were rechecked.

### Actual capacity and native results

The oversized Vulkan prepare passes original upload-destination validation, then reports G315, F316, M316, W3476 and N3792. Although N alone is below 4096, G+N is 4107: the original gate returns `ERR_PARAMETER_RANGE_ERROR` (5), reason 19, before publishing a ticket or facts. A normal G10/N132 prepare then succeeds using the same original allocations. Metal's existing conservative profile returns unavailable for that oversized request; the runner requires the exact backend-specific refusal, without weakening Vulkan's expectation.

Vulkan positive completes three distinct actual async readbacks totaling 44 bytes: sampled words `111,124,139,156`, two unchanged `a5a5a5a5` guards, zero FaultPlane and epilogue 1. Thirteen original facts cover uploads, Capture/Body, readbacks and native returns. The 64-byte image upload is observed in graph/frame 3 with native serial 7; Capture/Body/readback later occur in frame 5 with native serial 9. Six typed children expose distinct original physical free events and exact current-revision ACKs before original unit/storage retirement and guard unload. Metal positive independently returns the same exact bytes and typed physical ACKs.

The first Vulkan attempt (`085539`) aborted because the inherited control required immediate record success on any return. The original API may return `ERR_BUSY`/reason 19 while an earlier image-upload graph has not physically returned. The final driver retries only that explicit branch, observes the original ticket within a bounded budget, rejects failed/possible/unknown observations and preserves the same request/ticket. The final Vulkan log records two such returns before acceptance. The initial failed log/source/DSOs remain retained; no failure was erased or converted into success. Production ordering guards were not bypassed.

Typed prefix cancellation returns successful original upload facts, no readback/result, and six typed physical ACKs. Both V1 neighbors complete original exact ticket/readback or prepared-upload paths and physical release. Each process finishes ordered cleanup and DSO unload with exit 0.

## Independent review and remaining boundaries

`/root/vulkan_submission_bound_proof` independently derived the slot bound; `/root/astra_vulkan_retention_audit` reviewed the actual production/tests candidate and final source, artifact, log, DSO and container identities, finding no blocking omission under the existing success-only-return policy. `/root/vulkan_profile_runtime_review` reviewed the reused runner, refusal oracle and backend-origin pinning. The main coordinator separately checked ten source hashes and twelve raw logs and independently reran the 2-case/25-assertion native gate.

This accepts the original Vulkan qualification component path. It does not certify ANGE product identity/caller wiring, Sprite lifecycle pixels, WSI qualification, platform release/sanitizer coverage, automatic exit after an unknown native outcome or recoverable submit-device-loss behavior. The original unknown-retention and fatal submit-device-loss policies remain intact. Headless doctest results are not substituted for the separate actual main-RD driver evidence.
