# Image metadata error boundary fix — 2026-10-24

Commit `2ce45e3b9f` moves native session image metadata assignment until after the image texture format passes validation. A malformed/unsupported image now fails without publishing an image RID or format into the job result, avoiding stale metadata on an error path while RDPool remains the resource owner.

Static source checks and `git diff --check` pass. The optional module is absent from the existing binary; full build remains blocked by `No space left on device`; runtime/native/GPU/PCK/platform/production qualification remains false.
