# E06 shared witness and retained-core teardown repair — 2026-10-28

Status: original shared target actually built and loaded; narrowly reproduced native create-before-start teardown defect repaired. Full FR/queue remains active. Root priority now editor/Metal first chain; shared/static are not its prerequisites. No static build has begun.

## Editor first-chain handoff

Source44f32202b3, fixededitor d216693315918595bf7cf46d57006d88ede1d7000fe3cd68fac06afb63edffac and direct7field record/separateartifactreceipt remain frozen in work/e06-loaded-core-identity-20261028-r1/fixed-inputs/. work/EDITOR_MAIN_HANDOFF_20261028.json records absolute exact consumer paths/hashes; use frozen record, not global .gen selection overwritten by later variants. Fixed editor actual non-headless Metal/M3 Pro identity probe now rc0, exactclosedreadonly, originalfields. No actual GPU operation/completion/readback/Ready/G1 success follows metadata. A normal adapter can consume this now; no SDK builder here.

## Native defect and repair

Original libgodot_create_godot_instance calls Main::setup(false), retaining core services until start/setup2. Destroy without start called Main::cleanup, hit !_start_success and skipped unwind. Actual original shared instance read correct metadata then aborted at module exit with PagedAllocator/mutex errors (RUNTIME_VALID_PROJECT rc-6). Same module normal start/iteration/destroy returns0, isolating retained-core cleanup.

Main now tracks private NONE/CORE_ONLY/SECOND_PHASE at actual control-flow boundaries. Only successful retained core setup, !_start_success and no main loop selects the original cleanup_setup_failure(OK), which retains pending cancellation/preflight/guarded CORE and ordinary legacy unregistration boundaries. Entry into setup2 immediately sets SECOND_PHASE; neither its partial services nor normal started cleanup are redirected. No force cleanup, ownership/Ready grant, new registry or bypass. G1 refusal remains fatal before native teardown.

## Sealed actual shared evidence

- Original full shared build0,3127 compile actions/6m33; subsequent actual main archive/stamp/shared links0. No SDK/ANGE builder.
- Matching fixed-core-only-fix/libgodot.core-only-fix.arm64.dylib SHA5dd66b1c773521e21bf707e4e2259ba6ea53814a2ecbc2f6219924918b7d793e,302571912bytes. Separate closed CoreBuildSelection.shared.schema1.json and CoreArtifactReceipt.shared.json; full input bundle also frozen. Loaded core8b86252324832d9a54f2107d6a683b28fc9e550a873c233c66c371e63892f4b4/compiler6f015136c2a34384e255f71e67efd6b35388110d6c5656ba0d0cb4518f26120c, target SharedLibrary.
- Private host links only original C ABI declarations+Foundation, no SDK/corestamp values. dlopen actual module→original create→original Engine.get_version_info via supplied interface→exactclosedreadonly7fields→original destroy/dlclose. This proves module constants rather than OS executable path.
- SEALED_CORE_ONLY rc0: CORE init/deinit1/1, otherlevels0/0, destroy/dlclose0; no partial cleanup diagnostics.
- SEALED_STARTED rc0: original start/iteration; all4levels1/1, destroy/dlclose0.
- SEALED_EARLY_SETUP rc0: original --help returns no instance, no client initializer, dlclose0.
- SEALED_GUARDED_CORE rc0: real guarded DSO enrolled by ordinary project resource loader, SCENE not initialized; original SDK DSO finalizer observed before destroy returns. SceneReady is ABSENT because SDK hook registration occurs only during SCENE initialization. This is NOT actual Pending cancellation evidence; earlier PENDING_GUARDED file labels were inaccurate and are historical only.
- SEALED_PREFLIGHT_REFUSE rc78: real registered callback publishes and returns matched ERR_BUSY; exact raw GDEXTENSION_CLOSE_FAILED error=044 route=8. No destroy-return/guarded-DSO-finalizer follows. No native GPU work exists in this constituent refusal fixture; reported pending_count0 is not an invented completion.
- SEALED_INPUTS before/after15pins identical. SEALED_CLOSURE_AUDIT rehashes134 actual input/tool/recipe records and recomputes selection, all match. Frozen SDKarchivecb0fbc... reused only to link private guarded fixture; seven consumed SDKheaderpins checked unchanged against original frozen-client input pins. No SDK rebuild/source adoption claim.
- Independent read-only whole source+evidence review /root/indexed144_native_review: no concrete P1/P2; all15 runtimepins/134contentrows independently match.

## Retained failures and limits

First fixture lacked main scene, original setup alert timed out124; invalid acceptance, not metadata failure. Original valid create-without-start abort is behavioral RED. Initial refusal returned publishOK with diagnosticBUSY and was correctly rejected as mismatch/error002; corrected private callback returns matching disposition. Raw logs retained. Generic destroy/dlclose markers do not establish qualified E05/GPU physical retirement or OS unmapping. Repeated initialization unsupported; partial setup2 failure is source-separated but no new injected runtime witness. Guarded core arm has no initializedSCENE or actualPending hook. No normal ANGE target/submit/completion/readback/U08 success is claimed.

## Continuation

Natural safe-point delivery complete. Static aggregate repair feasibility reviewed, not executed; keep full native branch queue after first editor chain. Next priority per root is T1 r2 typed texture/sampler original producer/consumer/pinning and required normal start→execute→close gaps. Local test counts are not whole-product progress.
