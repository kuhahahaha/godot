# Native queue memory-visibility delta audit — 2026-10-24

Audited queue/callback memory visibility, idempotent close, and terminal error propagation.

No safe standalone production patch was found:

- Writer queue state, worker wakeups, poison transitions, descriptor retirement, and failure reads are synchronized through one mutex/condition-variable protocol.
- Capture and transport queues guard state reads/writes with mutexes; callback holders use the same lock while mutating pending/poisoned state.
- Writer close is idempotent: it marks closing, wakes the worker, joins at most once, and later calls observe the completed state.
- Short writes, `EPIPE`, malformed input, and invalid transport writes remain terminal and cannot be cleared by later calls.

The existing focused tests exercise broken receivers, poisoned drain, foreign-owner access, and close-after-publication. No source mutation was made.

Full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/production qualification remains false.
