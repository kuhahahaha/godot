# Native Cleanup / Retirement Slice Audit

Date: 2026-10-25

Audited owner-attributed native cleanup and retirement behavior outside the
already reviewed status, lifecycle, observe-free, codec-terminal, readback
overflow, RDPool partial-enqueue, draw-selection, phase-observer, write-census,
and typed-close seams.

Reviewed:

- `SkinnedMeshCaptureTestNativeSession::maintain()` holder sampling, pending
  cancellation, collector close/drain, pool release, and physical retirement.
- Readback/image callback holder accounting and duplicate/late callback poison
  behavior.
- `RenderCaptureTestAllocationRegistry` exact owner release, physical dispose,
  escaped-access retention, serial reuse, and concurrent allocation.
- RenderingDevice transfer-worker completion, recycle, command-pool reset, and
  poisoned-worker retirement paths.

The focused source and tests already cover the independent failure modes: late
or duplicate callbacks, undelivered request cancellation, holder-gated
retirement, exact allocation owner release, stale serial disposal, escaped
native access, typed completion mismatch, recycle failure, and reset failure.
No safe owner-attributed production patch was identified in this slice.

This is source/static evidence only. Full build remains blocked by the known
`No space left on device` archive failure. Runtime/native/GPU/PCK/platform/
performance/production qualification remains false; canonical acceptance rows
remain pending.
