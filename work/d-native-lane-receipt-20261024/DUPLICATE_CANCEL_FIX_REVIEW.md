# Duplicate readback cancellation fix — 2026-10-24

Commit `6f6d99b225` makes a second `cancel_unqueued(identity)` poison the collector. A canceled request is a terminal lifecycle transition; accepting another cancel as an ordinary parameter error would hide evidence corruption while the same callback holder remains live.

A focused test assertion verifies the second cancellation poisons the collector. Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
