# Delivery-holder error propagation blocker — 2026-10-24

Audited native-session delivery callback retirement. `release_delivery(publication, received)` decrements the holder under the session mutex and records `ERR_INVALID_DATA` plus `close_requested` when the callback was not received.

The publication parameter is intentionally not projected into a job result: a delivery can be destroyed before its pending requests are materialized as jobs, and the session-level `delivery_error` is the authoritative failure for that batch. Retrofitting per-job error assignment would require a new pending/publication error state and ordering contract.

Repeated holder release is impossible through the `Delivery` destructor ownership path; received callbacks mark both pending entries and materialized jobs before holder retirement. No safe standalone patch is justified.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
