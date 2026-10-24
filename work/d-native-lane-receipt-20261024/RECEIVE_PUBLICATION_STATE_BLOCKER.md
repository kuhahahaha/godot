# Receive/publication state propagation blocker — 2026-10-24

Audited native-session `receive()` and phase materialization.

The callback marks matching pending entries and already materialized jobs by exact publication id under the session mutex. The next pre-views phase materializes only received pending entries, copies publication/received state into each job, and erases the pending entry once. Unreceived entries remain pending and cannot be recorded early.

No safe standalone patch is justified. Adding a mismatch/unknown-publication poison path would require deciding whether late callbacks are expected holder acknowledgements or invalid batch receipts, which belongs to the broader delivery contract.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
