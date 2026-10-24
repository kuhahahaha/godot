# Readback mark-enqueued failure blocker — 2026-10-24

The downstream async readback path now propagates `cancel_unqueued()` failure when submission itself fails (`0ec406dbb6`). A remaining branch is `mark_enqueued(identity)` failing after `buffer_get_data_async()` has returned success.

This cannot be safely patched locally: once the RD call succeeds, the engine may retain the callback. The collector has no transition that records “callback retained but enqueue mark failed”, and calling `cancel_unqueued()` would treat a possibly retained callback as unqueued, risking double pending/holder retirement when the callback later arrives. Adding a poison-only method or a retained-but-unmarked state requires a broader collector/session contract and focused runtime coverage.

Disposition: precise blocker; no source mutation. Full build remains skipped and qualification remains false.
