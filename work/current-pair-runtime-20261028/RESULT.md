# Current pair runtime probe result

The new pair is operational at the source-admission/resource/unload boundary. Both valid probes exit 0 and the invalid VFX input rejects and rolls back with unload 0. No GPU/native particle execution, completion/readback or Ready claim is made. See `RECEIPT.json`, `SOURCE_MANIFEST.json`, and `INDEPENDENT_REVIEW.md`.
