# Publication error result propagation — 2026-10-24

Commit `5b52c33d13` writes `ERR_UNAVAILABLE` into the affected job result when raw/image transport publication fails, before failing and closing the native session. This aligns transport publication errors with snapshot, consumer, and image copy error propagation.

Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
