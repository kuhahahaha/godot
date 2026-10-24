# Image copy error result propagation — 2026-10-24

Commit `d07456e072` writes a non-busy image collector copy error into `job.result.error` before failing and closing the native session. This aligns image result propagation with the snapshot/consumer readback fix `7091c08232`.

Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
