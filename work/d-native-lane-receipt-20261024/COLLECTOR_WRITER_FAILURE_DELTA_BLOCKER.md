# Collector/writer failure-path delta audit — 2026-10-24

Audited close idempotence, post-poison calls, late-holder release, malformed callback receipts, and partial writer writes.

No independent production patch is justified:

- Collector `close()` is idempotent and admission remains blocked after close.
- Poisoned collectors reject subsequent copies/reservations while holder destruction still decrements the retained holder count.
- Late or malformed callbacks poison evidence; they do not certify a receipt.
- Writer short writes and `EPIPE` poison the worker, wake shutdown, close the descriptor, and clear queued lines. The byte counter is intentionally cumulative budget accounting and is not queue occupancy.

This audit does not alter the already-reviewed image close/drain fix or queue/release blocker. No source mutation was made. Full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/production qualification remains false.
