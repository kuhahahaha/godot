# Frame Deferred Disposal Audit

Date: 2026-10-25

Audited the owner-attributed RenderingDevice frame-deferred disposal path,
including `free_rid()` admission, dependency ordering in `_free_pending_resources`,
frame-window reset, destructive finalization draining, and the test-only
allocation disposal acknowledgement.

The implementation queues owned resources for the current frame, frees dependent
resource classes before buffers, drains every frame during finalization, and
resets `frames_pending_resources_for_processing` only after the queued frame is
fully consumed. Buffer allocation disposal is acknowledged only after the
original driver free and exact owner release. No independent safe production
patch was identified without changing broader renderer ownership semantics.

This is source/static evidence only. Full build remains blocked by the known
`No space left on device` archive failure. Runtime/native/GPU/PCK/platform/
performance/production qualification remains false; canonical acceptance rows
remain pending.
