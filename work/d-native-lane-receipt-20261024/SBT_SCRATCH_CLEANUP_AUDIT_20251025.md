# SBT / Scratch Cleanup Audit

Date: 2026-10-25

Audited raytracing SBT buffer creation and acceleration-structure scratch
replacement. SBT creation frees the native buffer on shader-handle or upload
failure and increments `buffer_memory` only after `_buffer_initialize()`
succeeds; pipeline creation frees the raytracing pipeline when SBT setup fails.
Scratch replacement queues the old buffer before installing a larger one and
accounts the new allocation only after successful creation.

No safe independent production patch was identified. Runtime/GPU receipts are
still unavailable and the full build remains blocked by disk space; all
qualification flags remain false.
