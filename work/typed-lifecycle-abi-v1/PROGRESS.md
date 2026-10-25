# Typed lifecycle ABI v1 implementation

Authority: ANGEParticleDesign/handoff/godot-fr/FOUR_SESSION_IMPLEMENTATION_PLAN.md, 2026-10-25 typed lifecycle ruling; revision3 FR_host_native_lifetime_PLAN §0.2–0.2b.

- Existing branch d/e06-policy-core; preserve mixed dirty implementation and untracked Registry dependencies.
- Generator failed on type `LevelNeedsV1[4]`; corrected to member name `needs[4]` and scalar type using existing generator support. Native C11 compilation confirms an actual inline array, not pointer. No generator change needed.
- Generated 15 lifecycle procedure signatures. Native arm64 layout: ABI 16, registration 136, diagnostic 32; needs offset 40, original initialization tuple offset 72.
- Regression added before proc registration: 15 lookups plus empty/nonempty/null/misaligned output checks. Red build running.
- Wrappers pending; public guarded loader remains disabled until joint qualification.

## Follow-through required by main session
- Implement real service action/schedule enrollment for actual ANGE SCENE needs (ResourceLoader, RenderingServer, CoreTypes); never replace actual needs with zero masks. Unknown service routes remain refused.
- Check declared entry classes against actual class/method/Callable/editor registration sites; numeric bit range alone is not qualification.
- Integrate C's precise original RenderingDevice epoch/session candidate when supplied, retaining D as sole core lifecycle writer. Never derive original keys from guard/RID integers.
- Apply reviewed Metal upload layout-transition patch at next builder-safe point; preserve fence/submit ordering and validate native startup before notifying C for its own native readback/retire probes.

- Generated wrapper implementation now in core/extension/gdextension_typed_lifecycle.cpp (15 adapters, compile-time signature/layout checks). Await current builder safe point to alias preflight type and register entry table. Main session directs batch validation; no additional full RED cycles.

## ABI milestone verified
- Three typed ABI cases / 102 assertions passed; original issuer 21 cases / 360 assertions passed.
- Final build exit 0. Native Metal4 Apple M3 Pro startup/script/exit passed, marker METAL_STARTUP_OK.
- Fresh reviewer found malformed ABI error classification and legal per-level diagnostics rejected; both corrected, malformed ABI and SCENE diagnostic exercised in passing tests.
- Root integration review requires legacy lookup to hide all 15 typed operations. Implemented at production gdextension_get_proc_address; metadata remains visible. Internal tests explicitly use TESTS_ENABLED-only lookup, no claim of production guarded loading.
- Generated header snapshot is consumable by A; complete loader activation remains pending service/census/native integration. No loader nonzero guard refusal removed.
- Changes to pre-existing untracked Registry files are preserved as exact milestone patches/snapshots; this is a mixed dirty engine build, not a clean standalone HEAD build.
