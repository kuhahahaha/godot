# Readback copy error result propagation — 2026-10-24

Commit `7091c08232` preserves the first concrete snapshot/consumer `copy_completed()` error in the job result before closing the session. Previously the session recorded only a generic failure and left the affected job's result error unchanged, obscuring the downstream contract state.

Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
