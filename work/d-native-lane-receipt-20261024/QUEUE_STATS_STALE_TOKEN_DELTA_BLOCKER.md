# Queue statistics and stale-token delta audit — 2026-10-24

Audited capture queue statistics, terminal error propagation, stale token handling, and physical disposal ordering.

No safe standalone production patch was found:

- Event sequence numbers are monotonic and independent from operation token and queue sequence identities.
- Completion consumes only an exact pending tuple; stale or mismatched completion poisons evidence and cannot retire another request.
- Poisoned capture remains drainable for already admitted completions, while seal still requires physical disposal and an empty pending set.
- Transport failures are retained as terminal state and prevent successful transport writes after failure.

Focused tests cover queue/token independence, stale completion, full-trace terminal poisoning, disposal ordering, and transport error retention. No source mutation was made.

Full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/production qualification remains false.
