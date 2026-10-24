# Cancel-unrecorded result propagation blocker — 2026-10-24

Audited the native session close path that converts pending requests into jobs and calls `cancel_unrecorded()`.

No additional source patch is justified:

- every unstarted job is marked `started`, `canceled=true`, and `error=ERR_SKIP` exactly once;
- already-started jobs retain their concrete record/readback/image/retirement error and are not overwritten by cancellation;
- pending delivery entries preserve publication and received state before cancellation;
- repeated close/maintenance passes do not re-cancel jobs because `started` is the guard.

Changing this behavior would risk replacing a real downstream failure with a generic skip. Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
