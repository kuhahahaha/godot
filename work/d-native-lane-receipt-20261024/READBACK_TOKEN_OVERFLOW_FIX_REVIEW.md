# Readback identity token overflow fix — 2026-10-25

Commit `aa0896ad85` tightens `resolve_readback_identity()` so `p_request * 2 + consumer` cannot overflow. The previous bound allowed `UINT64_MAX / 2`; consumer identities at that boundary wrapped to token zero and only failed later during collector admission. The resolver now rejects values above `(UINT64_MAX - 1) / 2` before constructing an identity.

Narrow verification: source assertion and `git diff --check` pass. A dedicated runtime fixture was not added because the resolver requires a live owner-thread RD pool; existing binary selectors are unavailable. Full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
