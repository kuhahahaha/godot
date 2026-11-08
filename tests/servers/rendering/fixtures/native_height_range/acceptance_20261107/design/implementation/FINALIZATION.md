# First-slice finalization branch, source-verified draft

No source integration or execution in this draft. Original source reviewed at current working-tree post-quote state; root owns final source pins.

Metal native_call_recycle rejects incomplete OR failed CompletionRecords (metal cpp130); native_call_release merely returns an observer, not the original resource owner (cpp183). Vulkan native_call_release similarly decrements observer count (cpp3481). Neither allows a Scene/native range to infer safe backing return from device-loss notification. FencePollResult::matches excludes native_error. There is no supported first-slice device-loss-to-complete shortcut.

Original RenderingDevice::finalize calls _begin_production_finalization, then _flush_and_stall_for_all_frames(false), then transfer-worker wait, then graph/resource/driver teardown. Failed frame or worker wait CRASH_NOW before dependent resource release. Original destructor checks _begin_production_finalization, calls finalize if needed, and require_close_success(native_finalization_returned ? OK : ERR_BUSY, LAST_REF) before member destruction. require_close_success is process fail-stop on error, not a warning.

## Required integration, no TODO escape hatch

1. Set an original RD native-height finalization-started flag at entry before new prepare/record is possible. Closing enrollment does not invent completion. Preserve cells/driver if production finalization refuses.
2. Keep original destructive frame/worker native failure fail-stop. Do not weaken that branch to discard cells or mark observers returned. Existing frame drain invokes both new retain barriers before any source retirement.
3. Record an RD finalization phase after original frame/worker drain succeeds. Before graph.finalize, resource sweep, queue free or driver_free, drain range observations and explicit returns using existing admitted native work. If release is BUSY, return from void finalize with driver, source rows and native_finalization_returned=false intact. A later explicit finalize resumes this phase; it does not re-run graph end/native issue. Normal destructor's existing LAST_REF guard fail-stops if this remains unresolved.
4. The helper _native_height_ranges_prepare_finalization returns OK only when all cells have no actual outstanding observer/source obligation. Empty no-effect or rejected-before-effect cells can settle without native complete. Actual unknown/failed/unmapped-prefix cells remain refused; no completed bool is supplied by caller. An actual failed native callback is retained as FAILED and does not release backing/claims.
5. Only after exact returns succeed may RD remove retained cells and proceed to original teardown. Cached successful final observation survives in client Refs; no raw RD/driver pointer is stored there and their destructors perform no native work.

A subtle retry trap: _flush_and_stall_for_all_frames(false) (cpp13494) is stall_previous -> _end_frame -> _execute_frame -> _stall_for_frame without an overall phase cursor. Treating final-stall BUSY as a retry of this whole sequence could re-end/re-issue. Retain BUSY during original destructive flush therefore keeps the original fail-stop behavior unless that full path is explicitly made resumable. The range-only post-drain phase must have its own cursor/phase and skip the flush on retry. This is narrower than changing engine-wide finalization.

## Mechanical counterexamples before root GPU acceptance

- Synthetic original-source native error: range observe FAILED, native_complete false, finalization helper refuses, exact RD driver pointer/cell remain, zero release calls.
- Completed source, first explicit return BUSY then OK: cursor remains on same row, successful release once, cached complete survives return.
- Post-drain finalize release BUSY: second finalize must not increment graph-end or issue counters; driver_free count remains zero until exact return succeeds. Requires test context/driver teardown stub in integration batch.
- LAST_REF with unresolved cell: subprocess must terminate through existing close failure or original destructive wait fail-stop; it cannot proceed to driver_free/member destruction. Requires a separate process mode; do not trigger fail-stop inside doctest process.

Normal/bounded seal/issue and both retain barriers are one atomic integration batch. This header/test draft alone is not implementation completion, RED evidence, ordinary-path acceptance, or GPU acceptance.
