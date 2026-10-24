# Readback cancel error propagation fix — 2026-10-24

Commit `0ec406dbb6` preserves the original `buffer_get_data_async()` error when cleanup cancellation succeeds, but returns the cancellation error when `cancel_unqueued(identity)` itself fails. This prevents a failed pending-accounting transition from being hidden by the earlier RD error.

The change is local to the downstream readback caller and does not alter physical retirement ownership. Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
