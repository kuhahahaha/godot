# P2 transfer/native completion — independent post-fix review

Date: 2026-10-24
Reviewer: independent post-fix source/evidence review (`/root/d_p2_independent_review`)
Scope: P2 transfer/native completion increment only; no production edits, rebuild, or matrix rerun.

## Disposition

**BOUNDED_APPROVAL_AFTER_AUTHOR_FIX.** The single prior review gap (no recording-plus-full-staging growth entry) is closed by the new seven-mode test and its recorded RED→GREEN mutation evidence. The scoped P2 evidence is acceptable for this constituent tranche only. It remains ineligible for overall acceptance, runtime credit, or production-ready status:

- `acceptance=false`
- `runtime_credit=false`
- `production_ready=false`

This is an independent review of the post-fix source and pinned evidence, not a claim of full K03/G1/G2/G3 or GPU qualification.

## Evidence identity

`FINAL_INPUTS.json` pins the live sources and host. The relevant post-fix identities are:

- `servers/rendering/rendering_device.cpp`: 481660 bytes, SHA-256 `d9af8ec5431d8c1a388ca68d7cbf1c044f4c7500c4baa27d6f05dbcad2c1ab3d`
- `tests/servers/rendering/test_rendering_device_transfer.cpp`: 24177 bytes, SHA-256 `0a60c6e19b179474f56442e1b3000ee8185b4e56541e5af4b8729efb8c9fa189`
- `bin/godot.macos.editor.dev.arm64`: 300757456 bytes, SHA-256 `f9238371862688472371f888c0fe0fa5a02ca4805089ff789f4b824ed7f5aa0d`
- `FINAL_MATRIX_RUNS.json`: SHA-256 `f4227b7d5628359da30d02d1d018fce2bec9434124fd7d0e4ca89279607bf140`

The recorded final runs are stable and exit 0: P2 21/333, Metal 5/202, Vulkan 1/27, E19 1/16, destructive finalize 4/4, and typed-close 7/7. The focused growth run records 1 case / 57 assertions, all passing. These are inspected as pinned evidence; they were not rerun for this review.

## Post-fix source review

The new test `[K03][P2] recording staging growth propagates its checked wait failure` (test source lines 418–470) creates a real `recording=true`, full staging worker and calls the production `_acquire_transfer_worker` path. Its seven modes are:

1. pending completion;
2. matching completion;
3. completed with wrong serial;
4. native completion error with code `-905`;
5. recycle failure;
6. command-pool reset failure;
7. rejected/non-issued submission.

The source path at `servers/rendering/rendering_device.cpp:7509–7548` ends recording, submits, and performs the required blocking observation before replacing the staging buffer. A failed observation returns before replacement (`7521–7525`), while a successful observation proceeds to replacement (`7528–7548`). `_submit_transfer_worker` (`7609–7619`) retains the complete queue report and marks rejected, unissued, or zero-serial submissions poisoned. `_wait_for_transfer_worker` (`7633–7668`) retains the typed completion record, rejects serial mismatch/native failure, performs one checked recycle, then one pool reset; it only clears the accepted prefix after all gates pass. `_release_transfer_worker` (`7572–7590`) drops the worker lock before crossing the pool mutex and notifies all on terminal retirement, preserving the pool→worker lock order and avoiding the prior lost-notification window.

The seven-mode fixture assertions verify the key branch result: matching completion is the sole successful replacement path; pending leaves the worker submitted and reusable later; all other modes refuse the acquisition and poison where completion/retirement is no longer trustworthy; rejected issue does not manufacture a serial. Existing P2 cases separately cover exact prefix retention, native-code retention, ordinary callers, tracker cleanup, command-begin failure, and public texture error propagation. The focused assertion-preserving mutant was recorded RED and the restored implementation GREEN in the frozen evidence set.

## Native source review

Metal, Vulkan, and D3D12 typed completion paths retain expected serial identity and distinguish native error, completion, and recycle. The reviewed implementation rejects armed-fence overwrite and serial exhaustion, owns no completion record for no-fence batches, and drains terminal Metal records before wrapper/scratch teardown. The Vulkan and D3D12 paths reject mismatched serials and use typed recycle after observed completion. These source conclusions are bounded by the runtime limitations below.

## Declined or untested branches

The following remain explicitly outside this disposition and must not be promoted:

- Metal injected terminal failure follows real native completion; it is not a hardware fault.
- No callback-pause publication stress test was run.
- Metal `native_code` is command-buffer terminal status, not detailed `NSError` data.
- Forced-barrier routing is not full untracked-resource qualification.
- Vulkan presentation-owned semaphore branch is source-reviewed only.
- D3D12 is source-reviewed only and was not compiled or run on macOS.
- Public holder/ObjectDB issuance, physical deletion/reclamation, and full K03/G1/G2/G3 lifecycle semantics remain outside this tranche.

No additional matrix or unrelated defensive work is required to close this single post-fix review finding. The remaining limitations are qualification boundaries, not a newly discovered blocker in the reviewed P2 increment.
