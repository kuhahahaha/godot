# E06 shared witness and retained-core teardown repair — 2026-10-28

Status: original shared target actually built and loaded; narrowly reproduced native create-before-start teardown defect repaired. Current shared rebuild also passes the original C ABI core-only, started, and early-help controls. Full FR/queue remains active. Root priority remains editor/Metal first chain; shared/static are independent native queue items.

## Editor first-chain handoff

Source44f32202b3, fixededitor d216693315918595bf7cf46d57006d88ede1d7000fe3cd68fac06afb63edffac and direct7field record/separateartifactreceipt remain frozen in work/e06-loaded-core-identity-20261028-r1/fixed-inputs/. work/EDITOR_MAIN_HANDOFF_20261028.json records absolute exact consumer paths/hashes; use frozen record, not global .gen selection overwritten by later variants. Fixed editor actual non-headless Metal/M3 Pro identity probe now rc0, exactclosedreadonly, originalfields. No actual GPU operation/completion/readback/Ready/G1 success follows metadata. A normal adapter can consume this now; no SDK builder here.

## Native defect and repair

Original libgodot_create_godot_instance calls Main::setup(false), retaining core services until start/setup2. Destroy without start called Main::cleanup, hit !_start_success and skipped unwind. Actual original shared instance read correct metadata then aborted at module exit with PagedAllocator/mutex errors (RUNTIME_VALID_PROJECT rc-6). Same module normal start/iteration/destroy returns0, isolating retained-core cleanup.

Main now tracks private NONE/CORE_ONLY/SECOND_PHASE at actual control-flow boundaries. Only successful retained core setup, !_start_success and no main loop selects the original cleanup_setup_failure(OK), which retains pending cancellation/preflight/guarded CORE and ordinary legacy unregistration boundaries. Entry into setup2 immediately sets SECOND_PHASE; neither its partial services nor normal started cleanup are redirected. No force cleanup, ownership/Ready grant, new registry or bypass. G1 refusal remains fatal before native teardown.

## Sealed actual shared evidence

- Original full shared build0,3127 compile actions/6m33; current shared rebuild `SHARED_BUILD_CURRENT_R2.rc=0` and no-op `SHARED_NOOP_CURRENT.rc=0` (`scons: . is up to date`). No SDK/ANGE builder.
- Current shared artifact after the builtin restore: `bin/libgodot.macos.editor.dev.arm64.dylib`, 302,572,232 bytes, SHA256 `5703b7bf571a3131439c29653b07fd0a1cb5bb26d5b12697f1ee58cef87ac8cb`; current direct selection is schema1, core `f90efb630ce9bb3f93aeaa28e9c780b2045e10cce72fa48ea09528b10f4ab447`, compiler `0d1bda555927e00eff753d861e8d67813b2c1bfddf0ce498aaf44243c5784994`, route `BuiltinGlslangV1`, target `SharedLibrary`. See `SHARED_CURRENT_ARCHIVE_CHECK_R2.json` and `SHARED_CURRENT_RUNTIME_INPUTS_R2.json`.
- Historical fixed-core-only-fix artifact and its separate closed selection/receipt remain preserved below; they are not silently repointed to the current artifact.
- Private host links only original C ABI declarations+Foundation, no SDK/corestamp values. dlopen actual module→original create→original Engine.get_version_info via supplied interface→exactclosedreadonly7fields→original destroy/dlclose. This proves module constants rather than OS executable path.
- SEALED_CORE_ONLY rc0: CORE init/deinit1/1, otherlevels0/0, destroy/dlclose0; no partial cleanup diagnostics.
- SEALED_STARTED rc0: original start/iteration; all4levels1/1, destroy/dlclose0.
- SEALED_EARLY_SETUP rc0: original --help returns no instance, no client initializer, dlclose0.
- SEALED_GUARDED_CORE rc0: real guarded DSO enrolled by ordinary project resource loader, SCENE not initialized; original SDK DSO finalizer observed before destroy returns. SceneReady is ABSENT because SDK hook registration occurs only during SCENE initialization. This is NOT actual Pending cancellation evidence; earlier PENDING_GUARDED file labels were inaccurate and are historical only.
- SEALED_PREFLIGHT_REFUSE rc78: real registered callback publishes and returns matched ERR_BUSY; exact raw GDEXTENSION_CLOSE_FAILED error=044 route=8. No destroy-return/guarded-DSO-finalizer follows. No native GPU work exists in this constituent refusal fixture; reported pending_count0 is not an invented completion.
- SEALED_INPUTS before/after15pins identical. SEALED_CLOSURE_AUDIT rehashes134 actual input/tool/recipe records and recomputes selection, all match. Frozen SDKarchivecb0fbc... reused only to link private guarded fixture; seven consumed SDKheaderpins checked unchanged against original frozen-client input pins. No SDK rebuild/source adoption claim.
- `NONBUILTIN_BUILD_CURRENT.rc` records the applicable fail-closed branch: identity generation writes schema0/unavailable (`nonbuiltin runtime compiler identity unavailable`), then the native module compile stops at the external dependency boundary because `glslang/Public/ResourceLimits.h` is absent. No schema1 stamp, Ready, or fallback identity is published. The builtin route was restored and revalidated by `SHARED_RESTORE_BUILTIN_R3.rc=0` plus the current core-only/start/early-help probes.
- Independent read-only whole source+evidence review /root/indexed144_native_review: no concrete P1/P2; all15 runtimepins/134contentrows independently match.

## Retained failures and limits

First fixture lacked main scene, original setup alert timed out124; invalid acceptance, not metadata failure. Original valid create-without-start abort is behavioral RED. Initial refusal returned publishOK with diagnosticBUSY and was correctly rejected as mismatch/error002; corrected private callback returns matching disposition. Raw logs retained. Generic destroy/dlclose markers do not establish qualified E05/GPU physical retirement or OS unmapping. Repeated initialization unsupported; partial setup2 failure is source-separated but no new injected runtime witness. Guarded core arm has no initializedSCENE or actualPending hook. No normal ANGE target/submit/completion/readback/U08 success is claimed.

## Continuation

Natural safe-point delivery complete. Current shared artifact and lifecycle controls are sealed; static macOS arm64 aggregate is independently delivered in `work/e06-loaded-core-identity-20261028-r1/`. Next native items remain T1 r2 typed texture/sampler adoption, K04 ports, nonbuiltin/unsupported linker branches, and the original owner joins for Host/Verified/U08/Ready/G1. Local test counts are not whole-product progress.
