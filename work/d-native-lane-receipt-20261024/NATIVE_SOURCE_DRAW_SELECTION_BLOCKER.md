# Native source draw-selection blocker — 2026-10-25

Audited the draw-selection and previous-view native source path as a separate cleanup/error surface.

The path fails closed on missing render-thread ownership, active lists, coverage poison, invalid scene/mesh/skeleton topology, uninitialized dispatch, escaped allocation, frame/viewport/mesh mismatch, stale allocation serial, unbound previous view, duplicate matching draws, and missing sequence. `capture_selected()` requires an exact observed draw and a bound previous role before entering capture.

No safe standalone patch is justified. Any change to selection ambiguity or previous-view admission would alter the source witness contract and requires owner/runtime evidence.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
