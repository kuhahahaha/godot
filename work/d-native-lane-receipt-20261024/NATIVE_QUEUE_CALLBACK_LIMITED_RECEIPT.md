# Native queue/callback limited receive — 2026-10-24

## Disposition

**APPROVE_LIMITED** for source-level native lifecycle evidence only.

The queue/callback seam has independently reviewed coverage for mutex/condition-variable visibility, close idempotence, broken receiver handling, foreign-owner rejection, poisoned FIFO drain, short-write terminal failure, exact callback identity, and late-holder release. The independent review is recorded in `QUEUE_MEMORY_VISIBILITY_INDEPENDENT_REVIEW.md`.

## Evidence set

- `QUEUE_MEMORY_VISIBILITY_DELTA_BLOCKER.md`
- `QUEUE_MEMORY_VISIBILITY_INDEPENDENT_REVIEW.md`
- `QUEUE_RELEASE_SEAM_BLOCKER.md`
- `QUEUE_STATS_STALE_TOKEN_DELTA_BLOCKER.md`
- `COLLECTOR_WRITER_FAILURE_DELTA_BLOCKER.md`
- focused source fixtures under `modules/skinned_mesh_capture_tests/tests/`

## Qualification boundary

This receive does not grant runtime, native binary, GPU, PCK, or production qualification. The optional module is absent from the existing focused binary, and the full build remains blocked by `No space left on device`.
