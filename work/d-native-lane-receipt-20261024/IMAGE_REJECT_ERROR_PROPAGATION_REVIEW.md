# Image rejection error propagation fix — 2026-10-24

Commit `b4fd7b953b` changes `SkinnedMeshCaptureTestImageCollector::reject()` to return `Error` and propagates a rejection failure from the native session's `texture_get_data_async()` failure path. Previously that cleanup error was ignored, allowing the session to continue with an unverifiable image slot.

A focused source assertion covers successful first rejection and invalid duplicate rejection. Static checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
