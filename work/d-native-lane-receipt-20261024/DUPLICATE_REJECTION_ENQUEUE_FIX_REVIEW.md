# Duplicate rejection/enqueue fixes — 2026-10-24

Two bounded native state-machine fixes were applied:

- `6ff3e53069`: image collector now poisons on duplicate `reject(slot)`.
- `e76041149e`: readback collector now poisons on duplicate `mark_enqueued(identity)`.

Both operations represent an already-consumed lifecycle transition. Returning only `ERR_INVALID_PARAMETER` would allow callers to continue while evidence integrity is no longer trustworthy; poisoning matches duplicate receipt, late callback, and malformed payload handling.

Focused source assertions were added for both transitions. The optional module is absent from the existing binary, and the full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.

Independent review: pending external short review; no full build rerun.
