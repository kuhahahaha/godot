# Collector cleanup seam review — 2026-10-24

The readback and image collector state machines were reviewed as an independent native cleanup seam. Callback holders are counted under mutex; malformed, duplicate, canceled, and late callback receipts poison the state; close blocks new reservations; readback drain requires both pending work and callable holders to reach zero; image rejection participates in drain. No safe production-code gap was found.

Existing binary selectors for this optional module are unavailable, so no runtime result is promoted. Full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/production qualification remains false.
