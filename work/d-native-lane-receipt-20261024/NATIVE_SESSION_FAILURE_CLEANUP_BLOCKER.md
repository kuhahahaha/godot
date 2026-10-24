# Native session failure-cleanup slice blocker — 2026-10-24

Audited the native session's combined transport/readback/image failure paths after the image reject and readback cancel propagation fixes.

No additional safe local patch was found:

- `fail()` preserves the first terminal error and records a distinct cleanup error separately.
- Image/readback resources remain owned by `RDPool::release()` after partial record failures; local frees would bypass the retirement ledger.
- `images.reject()` and `collector.cancel_unqueued()` now propagate cleanup failures at their downstream callers.
- A successful async submission followed by `mark_enqueued()` failure still requires the retained-but-unmarked contract recorded in `READBACK_MARK_ENQUEUED_FAILURE_BLOCKER.md`.

Disposition: precise blocker; no source mutation. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
