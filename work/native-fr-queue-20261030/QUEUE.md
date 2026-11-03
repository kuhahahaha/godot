# Godot native FR continuation queue — 2026-10-30

## 2026-11-04 Metal ordinary-frame seal candidate

The same-pair World trace proved the first `ERR_BUSY` comes from Metal copy-queue commit after one frame observer transfer. A source-matched Godot candidate lets ordinary frame preparation wait for that exact copy completion and retire it within the same seal; qualification maintenance stays nonblocking. Build rc 0, Metal doctests 3/3 and 63/63, visible GPUParticles2D rc 0. Trace-bearing matched pair passed visible Metal World/System/Cancel rc 0 without seal or present crash (94/0, 3/0, 3/0). Temporary trace code was removed and a new no-trace editor rebuilt (Metal 3/3, 63/63; broader Qualification 117/117, 43,082/43,082); A then rebuilt the DSO for the no-trace selection; visible Metal World/System/Cancel passed rc 0 (123/0, 3/0, 3/0), with no seal/present crash. Separate Direct and per-ticket product evidence remain open. Evidence: `work/particle-render-exit-seal-audit-20261105/SEAL_WAIT_CANDIDATE_RECEIPT.md`.

## 2026-11-04 production RD upload retry fixed-editor recheck

The current fixed editor SHA-256 `5feadf94...` reran the opt-in production main-RD particle retry on real Vulkan Mobile. Three buffer upload fault targets each failed twice, then recovered; the focused case passed 138/138 assertions with rc 0. This updates the binary-specific regression evidence without changing production source or claiming pixels or product six-link. Evidence: `work/particle-rd-upload-retry-host-20261103/RECEIPT.md` and `fixed-editor-recheck-20261104.log`.

## 2026-11-03 GLES3 mirrored 3D particle AABB

The GLES3 3D current-AABB path had the same positive-X activity assumption as 2D. A real OpenGL Compatibility mirrored-basis control read 8 moving particles from 512 bytes but produced a zero AABB and rc 1 on the fixed editor. The reader now checks the squared length of all three basis axes, matching the inactive zero-basis representation and handling mirrored transforms. A source-matched editor returned a nonzero AABB and rc 0; the earlier 2D rotated control stayed green. Evidence: `work/particle-gles3-aabb-live-20261103/MIRRORED_3D_AABB_RECEIPT.json` and `RECEIPT.md`.

## 2026-11-03 GLES3 rotated 2D particle AABB

The GLES3 current-AABB reader had treated a positive X basis component as proof that a particle was active. A real OpenGL Compatibility 180-degree 2D particle control read 8 moving particles from 384 bytes, yet the fixed editor returned a zero AABB and exited 1. The reader now recognizes nonzero 2D basis axis length, consistent with the copy shader clearing both axes for inactive particles. A source-matched editor returned a nonzero x AABB and exited 0 on the same control; the saved executable also reran successfully. The 4/4 GLES3 mode-layout fixture and 17/17 RD flag fixtures in both precisions passed. Fixed editor SHA was restored after the source-matched run. Evidence: `work/particle-gles3-aabb-live-20261103/ROTATED_AABB_RECEIPT.json` and `RECEIPT.md`. The original Method issuer and product six-link gate remain pending independently.

## 2026-11-03 live GLES3 particle AABB bounds

The fixed editor ran a normal OpenGL 4.1 Compatibility scene for 90 frames with both GPUParticles2D and GPUParticles3D. It exited 0 with finite, positive-area 2D/3D capture bounds and 512 bytes of 3D particle data. Replacing the default 128-by-128 gradient with an explicit 4-by-4 image changed the 2D capture rect from 128-by-128 to 8-by-8. The source layout fixture passed 4/4. This covers live bounds and 3D data, while individual 2D data layout and pixels remain unverified. Evidence: `work/particle-gles3-aabb-live-20261103/RECEIPT.md`.

## 2026-11-03 initial linear velocity texture contract audit

Source audit confirmed that `PARAM_INITIAL_LINEAR_VELOCITY` declares `linear_velocity_texture` but publishes no sampler RID and never samples it in generated shader code. The current scalar tests do not define texture behavior. No production edit or acceptance claim was made; the behavior decision and matching regression remain open. Evidence: `work/particle-field-texture-rebind-20261103/INITIAL_LINEAR_VELOCITY_CONTRACT_AUDIT_20261103.md`.

## 2026-11-03 current fixed-editor particle regression

The restored fixed editor SHA-256 `7cd3c520...` currently passes `[ParticlesReadback]*` 11/58, `[ParticlesProcessRetry]*` 3/47 and `[Particles]*,[GPUParticles]*,[CPUParticles]*` 66/334. Logs and rc files are in `work/particle-native-six-link-audit-20261102/`; this refreshes native controls without changing production source or claiming product six-link.

## 2026-11-03 Godot V2 production handoff recheck

The current Godot bridge already exports V2 prepare/record/observe plus V1 facts/copy/cancel/release on the same ticket. The fixed editor passed `[Qualification][MultiReadback]*` 8/317. The current ANGE integration adapter still calls V1 prepare/record/observe, and no V2 record production call or original request issuer was found. Exact signatures, source locations, SHA pins and owner action are in `work/particle-native-six-link-audit-20261102/GODOT_V2_PRODUCTION_HANDOFF_20261103.md`. No duplicate native interface was added or product six-link claimed.

## 2026-11-03 directed point emission zero normal

The 3D particle shader now sets initial velocity to zero for a zero directed-point normal, matching CPU 2D/3D. Real Vulkan Mobile shader-code RED 18/19 became GREEN 20/20 after the change; CPU initial-velocity process passed 2/2, particle neighbors 72 cases / 381 assertions, and sub-emitter RD regression 46/46. A live Vulkan Mobile GPU velocity readback is now RED on the fixed pre-fix editor (five non-finite zero-normal velocities, rc 1) and GREEN on the source-matched probe (five finite zero velocities and five nonzero-normal controls, rc 0). Source-matched probe saved and fixed Phase 1 editor restored. Evidence: `work/particle-emission-texture-rebind-20261103/ZERO_NORMAL_RECEIPT.md`. Pixels, other renderers and product six-link remain active.

## 2026-11-03 GPU sub-emitter runtime target replacement

`GPUParticles2D/3D` now follow target add, remove, rename and source path changes while the source remains in the tree. A real Vulkan Mobile backend RID test passed 46/46 after a failing removal/rename trial identified the required SceneTree event ordering; particle neighbors passed 71 cases / 379 assertions. Source-matched probe saved and fixed Phase 1 editor restored. Evidence: `work/particle-sub-emitter-post-enter-20261102/RUNTIME_RECEIPT.md`. Sub-emitted pixels and product six-link remain active.

Direct sibling sub-emitter paths (`../Target`) now skip unrelated `node_added` and `node_renamed` path resolutions. A tests-enabled count assertion was RED on the prior callback (2D 3→5, 3D 1→3) and GREEN with the filter; the live Vulkan Mobile backend test passed 48/48 and particle neighbors 72/381. Evidence: `work/particle-sub-emitter-post-enter-20261102/PERF_RECEIPT.md`. Deeper/absolute/unique paths still use the original callback, and high source-count mutation time remains unmeasured.

## 2026-11-03 particle parameter curve and shader variant rebinding

`ParticleProcessMaterial` now tracks in-place resource changes for parameter curves and the 3D rotation-velocity curve. Shared texture slots use a reference-counted connection and current-slot republishing because Godot signal identity ignores bound Callable values. A real Vulkan Mobile RED 14/16 became GREEN 21/21 after the fix; particle/ImageTexture neighbors passed 79/410 and ramp main-RD regression 23/23. Source-matched probe was saved and the fixed Phase 1 editor restored. Evidence: `work/particle-field-texture-rebind-20261103/CURVE_RECEIPT.md`. Pixel sampling, other drivers and product six-link remain active.

Next native contract audit: `PARAM_INITIAL_LINEAR_VELOCITY` currently declares `linear_velocity_texture` when its resource is set, but its setter does not publish a sampler RID and generated shader code does not read that sampler. Confirm the intended public behavior before changing this separate branch.

## 2026-11-03 particle ramp texture rebinding

`ParticleProcessMaterial` now tracks resource changes for color, initial color, alpha, emission and velocity-limit textures, detaches old subscriptions, and republishes material RID parameters. A cleared `Texture2DRD` publishes nil; rebuilding the same resource republishes the new RID. Dummy-host lifecycle passed 4/4, real Vulkan Mobile main-RD parameters passed 20/20, neighboring particle/ImageTexture cases passed 79/410. Fixed Phase 1 editor restored. Evidence: `work/particle-ramp-texture-rebind-20261103/RECEIPT.md`. Parameter curves, turbulence textures, shader pixel semantics and product six-link remain active.

## 2026-11-03 point-emission texture resource rebinding

`ParticleProcessMaterial` now tracks changed notifications for its point, normal and color emission textures, detaches replaced/destroyed subscriptions, and republishes current sampler RIDs. Clearing `Texture2DRD` in place resets its source RID and makes all three material texture parameters nil while disabling point emission; rebuilding republishes valid RIDs. Dummy-host connection lifecycle passed 4/4; a real Vulkan Mobile main-RD material-parameter probe passed 17/17; neighboring particle/ImageTexture tests passed 78/406. The fixed Phase 1 editor was restored. Evidence: `work/particle-emission-texture-rebind-20261103/RECEIPT.md`. Particle shader sampling/pixels and product six-link remain active.

## 2026-11-03 CPUParticles3D degenerate velocity alignment

The 3D CPU particle velocity-alignment branches now recover a valid basis from zero velocity and a degenerate prior basis in both free 3D and `DISABLE_Z` modes; a parallel prior X axis also gets a perpendicular tangent. The real native process test was RED with 5 failed axis checks, then GREEN 20/20 after the production edit. Source-matched neighboring CPU/GPU/particle cases passed 69/371. The fixed Phase 1 editor was restored. Evidence: `work/particle-cpu3d-degenerate-alignment-20261103/RECEIPT.md`. The broader native queue and GPU/product acceptance remain active.

## 2026-11-03 CPUParticles2D degenerate velocity alignment

Zero velocity combined with a zero prior transform basis no longer collapses the `PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY` particle transform. The process branch falls back to canonical up only at that degenerate boundary. Native RED failed both nonzero-axis assertions; source-matched GREEN passed 5/5, with the nonzero-velocity direction check, and neighboring CPU/GPU/particle cases passed 68/351. The fixed Phase 1 editor was restored. Evidence: `work/particle-cpu2d-zero-alignment-20261103/RECEIPT.md`. CPU-only transform behavior is verified; the broader native queue and GPU/product acceptance remain active.

## 2026-11-03 3D particle field texture clear synchronization

SDF collision and vector-field attractor nodes now listen to their current 3D texture's deferred change notification, detach replaced/destroyed subscriptions, and republish the field RID. Clearing `Texture3DRD` also clears its source RD RID; the nodes publish an empty field for that state rather than a 2D placeholder. Focused native RED 5/7 became GREEN 8/8; source-matched `[GPUParticles]*` 23/130 and `[Particles]*` 31/161 pass. A later source-matched real Vulkan Mobile main-RD probe directly read both collision RIDs' backend `field_texture`: clear, placeholder exclusion, valid 3D recreation, and old queued notification plus immediate replacement passed 25/25. See `work/particle-field-texture-rebind-20261103/RECEIPT.md`. Pixel sampling, other drivers, and product six-link remain open.

Successful `ImageTexture3D::create/update` now emits the missing resource change notification. A new native observer test was RED 3/6, then GREEN 6/6; the ImageTexture3D group passed 9/24. A wrong-sized update revealed an extra notification after backend rejection (RED 5/8); source-side validation removed it. The latest source-matched real Vulkan Mobile field-RID probe created and updated both slices with distinct RGBA8 sentinel pixels, read back the new bytes through main RD, and passed 42/42; neighboring GPUParticles and ImageTexture3D tests passed 32 cases / 156 assertions. See the same receipt. Particle shader field sampling and product six-link remain open.

The production RD particle processor now excludes SDF/vector-field collisions whose field texture is missing or not a valid 3D RD texture, preventing the default white sampler fallback from becoming an active vector force. A real Vulkan Mobile frame-params probe observed 1/1 → 0/0 → 1/1 counts; removing the guard failed exactly the two absent-field assertions (52/54). An active shared `Texture3DRD` was then cleared and rebuilt in place, producing 1/1 → 0/0 → 1/1 on the same collision instances; latest source-matched real RD test passed 66/66 and neighbors 63 cases / 317 assertions. See the field texture receipt. Particle motion/pixels, other drivers and product six-link remain open.

Compatibility now skips unsupported vector-field and 3D SDF entries before frame parameter counters advance. The original loop's compiled fixture is HEAD RED at 3/4 and current GREEN at 2/3 while supported sphere/box/heightfield entries remain. A source-matched Editor build, 54 particle doctests / 291 assertions, and the 230-frame OpenGL Compatibility lifecycle passed. A live 60-frame scene with both unsupported fields returned nonempty particle data and a finite AABB, with the expected warning and rc 0. See the field texture receipt. Direct live GLES3 frame-counter inspection, field pixel/motion semantics and product six-link remain open.

## 2026-11-03 GLES3 history buffer zero-name rollback

The two-buffer history allocator now rejects either zero GL name and frees a partial first allocation. The update branch queues failed nodes for a later pass before the swap/copy. A compiled fixture executes both the original allocator and the extracted `update_particles()` history branch with first/second zero-name injection; HEAD is RED and current source GREEN for rollback, no copy/swap, one retry, then a successful copy. Neighboring GLES3 retry fixtures, source syntax and formatting pass. A source-matched editor compiled the changed GLES3 source; `[Particles]*` 31/161, `[GPUParticles]*` 22/122 and the 230-frame OpenGL Compatibility lifecycle pass. The fixed Phase 1 editor was restored. Live OpenGL fault injection remains open. See `work/particle-gles3-history-allocation-20261103/RECEIPT.md`.

## 2026-11-03 restored editor build identity correction

The restored fixed editor SHA-256 `7cd3c520...` had a sidecar with stamps from a later test build. Its actual `Engine.get_version_info().controlled_build_identity` output was captured and the sidecar selection corrected to match all fields, size and hash. See `work/particle-build-identity-sidecar-20261103/RECEIPT.md`. This is evidence repair only.

## 2026-11-03 RD heightfield caller current-binary validation

The RD heightfield empty-framebuffer caller guard now has current-editor build/regression evidence: SCons target up to date on editor SHA-256 `7cd3c520...`, compiled handoff 2/2, RD extents 11/11, `[Particles]*` 31/161, and `[ParticlesProcessRetry]*` 3/47. See `work/particle-rd-heightfield-failure-20261103/RECEIPT.md` and raw logs in `work/particle-rd-heightfield-followup-20261103/`. Live RD allocation failure remains untested; product six-link remains pending original owner input.

## 2026-11-03 GPUParticles3D trail override mutation warning

An in-place trails-flag mutation of the current material override now emits a material change that refreshes the edited GPUParticles3D node warning. Replacing the override detaches the old signal and attaches the new one. Focused SceneTree RED 1/2 assertion failure became GREEN 5/5; neighboring `[GPUParticles]*` 22/122 and `[Particles]*` 31/161 pass. The test editor is retained separately and the prior Phase 1 editor hash `7cd3c520...` is restored with a matching sidecar. See `work/particle-trail-override-mutation-20261103/RECEIPT.md`. Product six-link and live trail rendering remain open.

## 2026-11-03 GLES3 zero-name heightfield current-binary validation

The previously source-only `glGenTextures`/`glGenFramebuffers` zero-name follow-up now has current tests-enabled editor coverage. Source fixture 10/10, `scons platform=macos target=editor tests=yes -j2` rc 0/up to date, `[Particles]*` 31/161 and `[GPUParticles]*` 22/118 passed on editor SHA-256 `7cd3c520f7f643f611693768dafe156264110a234c2d1babec372c95c75e9665`. OpenGL 4.1 Compatibility completed the existing 230-frame lifecycle with all eight checks. See `work/particle-gles3-heightfield-failure-20261103/RECEIPT.md` and raw output under `work/particle-gles3-heightfield-followup-20261103/`. Live forced GL allocation failure and product six-link acceptance remain open.

## 2026-11-03 production RD particle upload retry host

An opt-in normal-startup test host now executes the three particle upload retry fault boundaries against the main Vulkan RD and production compositor. Vulkan Mobile on Apple M3 Pro completed rc 0 with 138/138 assertions; Metal, headless, and separate-render-thread controls refused with rc 1. A later neighboring test run overwrote the fixed Phase 1 editor path with the new probe binary after its backup was removed; the fixed pin is currently unavailable and must be restored from verified bytes before further fixed-pair runs. See `work/particle-rd-upload-retry-host-20261103/RECEIPT.md`. This is a native retry constituent, not the product same-ticket six-link acceptance.

## 2026-11-03 CanvasItem material mutation warning refresh

CPUParticles2D/GPUParticles2D now refresh edited-scene warnings when an assigned CanvasItemMaterial changes its particle-animation flag in place. CanvasItem disconnects the old material and connects the replacement, so stale resource changes do not notify the node. The native test was RED 0/2 and GREEN 5/5; `[GPUParticles]*` passed 21/104 and `[Particles]*` 31/161. The fixed editor was restored and HEAD unchanged. Evidence: `work/particle-canvas-material-mutation-warning-20261103/RECEIPT.md`.

## 2026-11-03 owner-package three-scenario review control

The current A integration HEAD `1fa9ea870` reports E05 Metal SceneReady rc 0 but still rejects qualification at `host.qualification_request_issuer`; no authentic success/failure/cancel owner package is present. The Godot capture harness now has a post-capture review command that verifies evidence integrity and fixed tuple agreement, compares raw bytes with the pinned independent oracle, and leaves same-ticket completion, physical release and drain explicitly pending. The combined Python suite passes 41 tests. See `work/particle-native-six-link-audit-20261102/PHASE1_INTAKE_20261103.md` and `RUNBOOK.md`. No product six-link acceptance is claimed.

The capture bundle now retains exact request and independent oracle bytes before launch; review compares its raw readback against the saved oracle rather than reopening an external file later. A tampered saved oracle fails review, while deleting the external oracle after capture does not alter the saved-byte verdict. The combined Python suite passes 45 tests. Product issuer and same-ticket completion/release/drain remain pending.

Two further RED controls showed a request changed during snapshot copy and a copy failure still launched the child. The capture controller now rechecks every input immediately before launch and records a no-child rejection on either condition; all 47 capture/owner/review controls pass. The authentic owner must still guarantee the request actually consumed after launch matches its retained source.

The review CLI now returns rc 1 and `REVIEW_FAILED` for a raw readback/oracle mismatch even when the capture bundle itself is intact. A CLI control was RED before the fix and GREEN afterward; the combined suite passes 48 tests. Matching bytes remain uncorrelated until the original issuer and same-ticket completion/retirement evidence are reviewed.

## 2026-11-03 CanvasItem material warning refresh

Replacing a CanvasItem material now requests a configuration-warning refresh for edited 2D particle nodes. A native SceneTree regression was RED 0/2 and GREEN 2/2; `[GPUParticles]*` passed 21/104 and `[Particles]*` 30/156. The fixed editor was restored after the tests-enabled build. Evidence: `work/particle-canvas-material-warning-sync-20261103/RECEIPT.md`.

## 2026-11-03 material override warning refresh

`GeometryInstance3D::set_material_override()` now emits a configuration-warning refresh after publishing the effective material. A native edited-SceneTree regression was RED for the missing `node_configuration_warning_changed` signal and GREEN 1/1 after the fix; `[GPUParticles]*` passed 21/104 and `[Particles]*` 29/154. The fixed editor was restored and HEAD unchanged. Evidence: `work/particle-material-override-warning-sync-20261103/RECEIPT.md`.

## 2026-11-03 GPUParticles3D trail override warning

The trail-material configuration warning now evaluates an effective `BaseMaterial3D` override instead of retaining a missing-trails result from a covered mesh material. A native regression was RED 1/2 and GREEN 2/2; the tests-enabled editor passed `[GPUParticles]*` 20/103 and `[Particles]*` 29/154. The fixed Phase 1 editor was restored after the test build; Godot HEAD is unchanged. Evidence: `work/particle-gpu3d-trail-override-warning-20261103/RECEIPT.md`. Live GPU trail rendering and product six-link acceptance remain separate.

## 2026-11-03 RD heightfield allocation-failure handoff

The RD heightfield storage getter already returns an empty RID on texture/framebuffer allocation failure, but the scene renderer passed that RID into Mobile/Forward+ setup. The caller now skips the render on an empty RID. A compiled production-handoff fixture was RED (`failed framebuffer reached renderer`) and is GREEN 2/2; the neighboring RD extent fixture is 11/11, and the changed caller passes source syntax compilation. No live RD allocation failure was injected. See `work/particle-rd-heightfield-failure-20261103/RECEIPT.md`.

## 2026-11-03 GLES3 heightfield creation failure continuation

The GLES3 particle heightfield now checks framebuffer completeness in every build, retires failed texture/framebuffer handles, clears cached size, and returns zero; the scene renderer skips a zero target. The new source fixture went RED 0/8 to GREEN 8/8, and the normal OpenGL 4.1 Compatibility lifecycle smoke completed 230 frames with zero script failures. The changed GLES3 files compiled in a tests-enabled editor; `[Particles]*` and `[GPUParticles]*` passed 29/154 and 19/101. The fixed Phase 1 editor was restored to SHA-256 `1fe3994f54e8f74476ed97a967d919ecfc832886e8aea971b79003cb9f5c6a9b`, HEAD unchanged. See `work/particle-gles3-heightfield-failure-20261103/RECEIPT.md`. Actual forced GL allocation failure and product six-link acceptance remain open.

A follow-up review found that zero returned by `glGenFramebuffers` could be mistaken for a complete default framebuffer and cached alongside the new texture. The getter now rejects zero texture and framebuffer names before binding and leaves the texture retryable after framebuffer allocation failure. The expanded fixture was RED 8/10 and GREEN 10/10; source syntax compiled with the installed macOS SDK. This follow-up did not rebuild the editor or inject a live GL allocation failure.

## 2026-11-03 CPU3D particle animation warning continuation

The CPU3D configuration warning now retains an animated surface result when later mesh surfaces use ordinary materials, evaluates a valid material override as the effective material, and recognizes `ORMMaterial3D` through its `BaseMaterial3D` billboard mode. The two-surface doctest failed 1/2 before the first fix, then exposed the override failure at 3/4 and ORM failure at 4/6, and passed 6/6 after all three source fixes; neighboring `[Particles]*` and `[GPUParticles]*` selections passed 29/154 and 19/101 on the rebuilt test editor. The fixed Phase 1 editor was restored to SHA-256 `1fe3994f54e8f74476ed97a967d919ecfc832886e8aea971b79003cb9f5c6a9b` without moving HEAD. See `work/particle-cpu3d-animation-warning-20261103/RECEIPT.md`. The E05 owner input and original product six-link gates remain open; this local fix does not close the long queue.

The user's requested long goal covers all authorized Godot-side work in the original FRs, including native integration and cross-cutting work that can advance independently. This queue supplements the older evidence ledger; its old editor hashes and historical status paragraphs remain tied to their original receipts. A blocked owner or CI row is not completion of the queue. No scheduled task is used. An earlier capture observed `blocked`; the current Codex goal was verified `active` on2026-10-31. Earlier status paragraphs below retain their historical scope.

| Workstream | Current evidence and next work | Scope boundary |
| --- | --- | --- |
| K03, typed close and native issue/completion/readback | QRT candidate `cb3d43...` received peer review, passed a fixed-source incremental editor build and was reported integrated on the shared branch. Snapshot zero-capacity overlap repair was built/tested as `ebc124...`; it is replayed as local candidate `7a64a659...` on public parent `1655f55...`, with project static hooks passing to a fixed point; its incremental editor build, focused native regressions, Vulkan driver controls and Metal ticket/upload fixtures pass; it has not been pushed. The d23 native Metal fixture also exercises same-ticket completion and, separately, prepared segmented upload and record; continue actual product operation/issuer, completion, exact readback and retirement seams rather than substituting fixture identities. | `work/qrt-malformed-alias-20261030/RECEIPT.md`, `work/qualification-zero-snapshot-20261030/RECEIPT.md`, `work/d23-qrt-native-ticket-20261030/RECEIPT.md`. Keep fail-closed and G1 unchanged. |
| Fixed ANGE pair and U01/VFX | The `d23e0b...` candidate editor plus the retained fixed DSO SHA `a8511912...` and RmlUi were run with non-headless Metal; VFX and no-VFX stop at `host.storage_owner_transport / E05` before world entry. The earlier coordinator report associated that retained DSO with an `fd33...` baseline, but loaded source identity is not joined and the current ANGE checkout has a different HEAD. Continue native work independently and rerun the product path when real owner input is delivered. | `work/d23-fd33-metal-pair-20261030/RECEIPT.md`, `work/cb3-fd33-metal-pair-20261030/RECEIPT.md`. No Ready, world entry, close or GPU acceptance is inferred. |
| P01 collectors and native lifecycle | The image/raw collector drop fix was integrated into the `0fc...` parent and validated under module-on builds; ordinary callback resource lifetime and other native closures retain their own receipts. Continue the full P01 lifecycle, external acquisition and actual GPU obligations. | `work/p01-collector-drop-20261030/RECEIPT.md`, `work/uniform-set-invalidation-lifetime-20261030/RECEIPT.md`. Fixture readback is not product owner proof. |
| Physics 3D/2D and shutdown | Real 3D fixture selects and checks both built-in backends in both thread modes. Local Jolt job-system fix `e5bc8f4b8f` closes a tested worker-release/allocator seam; complete wrapper park/admission/finish, query retirement, scene continuations, shape/joint/soft-body and adopted 2D policy remain outstanding. | `work/physics-query-fixture-20261030/RECEIPT.md`, `work/jolt-job-retirement-20261030/RECEIPT.md`; adopted FR/PLAN under `Dev/ANGEParticleDesign/docs/contracts/native-physics-cleanup/`. The narrow Jolt fix is not full physics acceptance. |
| Rendering/device physical retirement | Framebuffer same-RID callback reentry is locally repaired as `d870ac2787`; parent/sibling cascade reentry is locally repaired as `34f8d55c61`, and late busy after a nested callback as `821a669495`, each regression-tested. A public-parent `61d` replay `970514604e` has its own incremental build and tests; the peer reports independent review and public push. Its successor `ecc5ff5a04` replays the synthetic pipeline enqueue-refusal guard on public parent `8283`; its own incremental build and regressions pass, but independent review remains open: synthetic global-flag pending-owner controls fail; direct owner method controls pass. The bounded candidate `d23e0b5486` scopes retirement to the original cell/RID, passes owner, controlled-driver reentry and related regressions, received independent review, and was pushed; its static-check corrections are reported at `181fc83...`; real backend callback behavior remains unproven. Keep the existing checked native-return result and actual parent-resource retention requirements; the Vulkan descriptor unknown outcome cannot be treated as successful retirement. Continue independent Godot source and failure-path work without fabricating completion. | `work/d23-native-readback-20261030/RECEIPT.md`, `work/rd-retirement-cell-scope-20261030/RECEIPT.md`, `work/rd-pipeline-review-20261030/RECEIPT.md`, `work/rd-pipeline-8283-20261030/RECEIPT.md`, `work/rd-pipeline-enqueue-refusal-20261030/RECEIPT.md`, `work/rd-cascade-61d-integration-20261030/RECEIPT.md`, `work/rd-late-busy-20261030/RECEIPT.md`, `work/descriptor-return-native-20261029/`, `work/descriptor-return-165-red-20261030/RECEIPT.md` and current device-exit/qualification source. Full dependency, replay, teardown and backend results remain to be verified. |
| K02 ordinary MultiMesh and canvas | Fresh cached motion and hidden cross-frame carry repairs are public at04a. New267 fixture plus759 mode-only successor adds actual 3D/color/custom→2D/no-fields canvas layout, visibility/move/hold coverage:16real Metal Editor/release precision/renderer cells, exact control census, complete data and independent PNG/PCK review. Hosted267 failed the tracked executable-mode hook; mode-only759 successor CI41134479049 now passed static,2+21Python and seven native groups, with recorded diagnostics. | `work/k02-native-layout-canvas-20261031/RECEIPT.md`; canvas direct draw and native command state remain distinct; wholeM12/B4/E34/owner/typed-close/G1 remain open. |
| Active Reload, C4, native scene/viewport and B4 | Existing narrow GDScript Active Reload/Cancellation/Lifecycle controls pass. Four isolated C4 tests show cancellation and instance teardown release captured values under the language lock, including connection-only state ownership; continue the full safe ownership handoff, not just an unlock. Continue each original FR against its latest adopted contract and exact source. Local gate failures limit their own rows only. | Relevant current work/evidence packages and the Godot FR handoff remain authoritative; no whole-family status is inferred from this queue. |
| Cross-platform, CI, sanitizer, release, packaging and performance | Maintain exact source/binary identities and run applicable checks when prerequisites are available; a local debug pass cannot substitute for release, sanitizer, platform or public CI. Public `d064b9bf` CI run `41002290766` passed static and Linux sanitizer smoke. The subsequent `d23e0b` run `41006634414` failed static checks before Linux; the `181fc83` CI also failed; public `1655f55` run `41007283432` was observed in progress. | Each result remains scoped to its actual run; no product/GPU qualification is inferred. |



## Continuation — K02 M11 MultiMesh failure cleanup, 2026-10-31

Godot commits `973caa08d3` and `a91eb5262e` add TESTS_ENABLED-only failure controls and a real small Vulkan regression for M11 instance-buffer creation, indirect command creation, and visible-count command update failures. Each path invalidates stale GPU resources and notifies MultiMesh dependents. The final macOS test-enabled Editor rebuild after the helper follow-up completed with rc0; focused M11 passed 1 case / 18 assertions and the `[B4][MultiMesh]*` neighbor selection passed 2 cases / 30 assertions. Expected controlled diagnostics are retained. Evidence is in `work/k02-m11-failure-20261031/RECEIPT.md`; the candidate remains local and is not a full M11/physical-retirement/owner/G1 qualification. Continue the wider K02/M12/B4 queue independently.

The next implementation task is selected from these lanes using actual current source and adopted contracts. New evidence is recorded under its own fixed identities; author verification is not labeled independent review.

## Continuation — R01 qualification ticket native frame error, 2026-10-31

Personal Godot head `395cf18c1e` follows the original frame native-source fix `30548f251c`. A red/green older-frame regression proved that a qualification ticket native poll failure left the original buffer readback `OK` and allowed a second backend poll. The successor closes the actual frame/readback, captures its native code and preserves the ticket's own error fact. Both single and double macOS Editor builds and the focused 156-case / 3,931-assertion set pass; source identity, hashes and limitations are in `work/r01-qualification-frame-error-20261031/RECEIPT.md`. Hosted CI for the parent `30548f251c` is now terminal success with static checks, 2+21 Python tests and seven native groups; successor CI remains pending. Production operation/source association, physical retirement, owner ack, typed close and G1 remain open. Continue the broader original Godot FR queue.

The next remote head `d5cd3c32dc` closes a distinct native-code preservation gap: the original readback result now retains its source code when marked failed, so a later ticket publication cannot replace the code with zero. A second red/green 13-assertion control and single/double 156-case / 3,933-assertion groups pass; full details are appended to the same receipt. Hosted `41161355562` for `395cf18c1e` is now terminal success (2+21 Python, seven native groups, 271 cases / 72,973 assertions). Hosted `41161741758` for `d5cd3c32dc` remains pending. Neither native unit control is product owner/source/close or G1 proof; the long queue stays active.

The subsequent exact head `e5ed473e3c` closes the final native fence poll's original-frame failure boundary. Red/green double control proves a one-time native fence error previously allowed the same readback to remain `OK` and the backend to be polled twice; final double 156-case / 3,937-assertion group passes. The parent `d5cd3c32dc` hosted `41161741758` is terminal success (2+21 Python, seven native groups, 271 cases / 72,975 assertions); `e5ed473e3c` CI remains pending. Single-precision objects were cleaned in the isolated native worktree after a disk-full double archive failure; no single-precision result is claimed for `e5ed473e3c`. See `work/r01-fence-frame-error-20261031/RECEIPT.md`. Continue all independent Godot FR work; original production join/owner/typed-close/G1 remain open.

## Continuation — B4 material cache, 2026-10-30

Independent local candidate in `/private/tmp/godot-b4-live-material-20261030` checks both cached and current material generation in Mobile and Forward+ for external packets, with red/green packet-level regression and ordinary Indexed144 Metal controls. This is uncommitted pending a clean build, direct external renderer integration exercise, and independent review; RID/generation is not E12 identity or a lease. Detailed evidence: `work/b4-live-material-20261030/RECEIPT.md`. Continue other FR lanes independently; this one is not whole-queue completion.

Public parent `7a64a659...` CI run `41011677897` completed successfully for static checks and Linux sanitizer smoke, with platform jobs skipped; it does not validate the candidate. Disk free space fell to roughly 300 MiB while multiple activities were present, so further large builds need safely recovered capacity.

The current public-parent 3D wrapper was separately checked against the adopted physics residual PLAN: `_thread_sync()` only sets `doing_sync` inside a queue flush; `_thread_loop()` has no post-full-flush park acknowledgement, and `finish()` queues backend finish/exit without the specified admission and drain protocol. These are concrete remaining R1 work, independent of B4. The adopted plan also requires the complete 3D backend and 2D matrices; the local Jolt worker-retirement fix is narrower. The current capacity constraint prevents a new test-enabled engine link; it does not change these rows to complete.

Independent B4 review found that packet identity is currently shared over the scene batch and list entries admitted before a later mismatch are not withdrawn; shadow paths also do not carry an external identity. The live-material candidate is therefore explicitly **not integrable as end-to-end packet validation** until per-output association and atomic list admission are implemented. Material generation saturation and dependency stamps also remain open. See the current receipt for exact scope.

Narrow saturation rejection and its registered test are isolated in local commit `f7605efefc1362d7288ea3b309295f0b114ebe9b` after the two local material commits. The actual inline predicate passed a standalone red/green control and independent review; the registered doctest has only compiled, pending a test-enabled link, so the commit is a candidate and is not a product integration or E12 claim. See `work/b4-generation-exhaustion-20261030/RECEIPT.md`.

After space recovered, f760 registered doctest ran in a controlled overlay: focused 1/4, B4 24/517, RenderingDevice 18/263 and shader 17/5754, rc0, stable link inputs. This is not a full clean candidate build or renderer external-packet test.

The current source audit locates the single scene-batch RenderDataRD, separate shadow append ranges, unretained material data and PRE_OPAQUE callback boundary: `work/b4-render-list-owner-audit-20261030/RECEIPT.md`. Independent review found a single-fill preflight can be narrowed but cannot provide per-output or cross-pass atomicity; a real renderer regression is in development.

Local commit `3787d88afb` adds an independently reviewed single-fill batch preflight; real Mobile/Forward+ list tests observed red→green on a controlled fixture, not product owner, cross-pass or lease qualification. Registered material tests are locally integrated as `a136f4a334` with red/green GPU controls, with follow-up review fixes for skipped reporting and early cleanup in progress. Exact evidence: `work/b4-batch-preflight-20261030/RECEIPT.md`, `work/b4-persistent-material-tests-20261030/RECEIPT.md`, `work/b4-render-list-fixture-20261030/RECEIPT.md`.

The renderer fixture was integrated locally through `5731d6b343`, including the SCU directory and marker-parser fixes; its final uniquely named test entry remains unlinked/unrun. The material test review follow-up is integrated as `5a0d3f11d0` and has compile/skip evidence only for its revised cases; actual Vulkan execution and independent RID cleanup assertions remain in progress. Bounds single-box patch is uncommitted WIP; reviewer found aggregate overflow still open. ArrayMesh E25/E20 source/version seam is now a separate active native workstream, without treating changed notifications or existing fixed surface numbers as authenticated content revisions.

E25: actual surface-conversion reuse left stale index provenance; source-only candidate compiled and independently reviewed, with real red and green link pending. ArrayMesh signal/preview counters, RD fixed surface numbers and E20 capture reservation do not provide the real MeshStorage creation mirror, source grant, E04 window or original owner byte budget required by MP1. See `work/e25-provenance-reuse-20261030/RECEIPT.md`.

Test cleanup follow-up `5d89aa3ec8` individually checks each RD RID owner after fixture cleanup; it has compile and no-Vulkan skip evidence, with the revised real Vulkan test still pending. The full bounds candidate now preflights existing and merged AABBs transactionally; real RED is recorded, GREEN has compiled but not linked due low disk, so it remains uncommitted. E25 owner analysis additionally found `CapturePayloadPlan::source_staging` empty for non-E21 and no original Mesh E04 grant; byte ownership and source-window integration remain required.


## Executed continuation — 2026-10-30, B4 and E25

The current isolated clean candidate is `e10ce1b67c94f9c7ed344b98b9e2733a8f215949`, pinned by `refs/codex/b4-e25-candidate-20261030` and a verified source bundle. E25 stale-provenance clearing is local replay `57b928c309` (original independent commit `a58f5df8a9`), 19/19 assertions in its focused control. Bounds finite/nonnegative base-aggregate preflight is e10ce1b67c: identical expanded GPU test has RED 20/143 failures against prior single-box logic and GREEN 142/142; normal union and existing ordinary/valid surface state controls are included. Independent read-only review found no explicit P1/P2 blocker for these two commits.

Final uniquely named renderer fixture now executes RED→GREEN on actual Metal Mobile and Forward+. Final revised material tests run 3/92 on real Vulkan, after correcting an incremental-build header mismatch; the failed mismatched-header attempt is retained and not counted as passing. A combined controlled incremental host passes 30 cases / 639 assertions (new B4 MaterialGPU + E25 + K03 P2) and the same final Metal fixture on both renderers. Static checks pass, and source worktree is clean. No clean full candidate engine build or public push is claimed. Exact inputs, commands, source series, bundle, binary hashes and logs: `work/b4-e25-combined-20261030/RECEIPT.md`.

E05 staging zero-value feedback was rechecked: the current ancestor d93206615 already implements normalization and focused regression; the prior UINT32_MAX statement was incorrect and was withdrawn. No duplicate source change was added. Actual project-config-to-texture-transfer coverage remains distinct.

All remaining original FR scope stays in the queue. In particular, these controls do not complete actual E12 output/colour/shadow association, material lease and conservative bounds arithmetic, or E25 original immutable mesh mirror, source grant, budget and E04/E20 integration. Physics work interrupted earlier remains unimplemented in this continuation. Current work remains ordinary rendering, numerical validation, resource conversion, tests and evidence upkeep; no security-research task is being pursued.


## Coherent candidate build — 2026-10-30

The next local commit908030e5dc admits the actual no-operand linker strip option -s into the original encoded build inputs, retaining exact argument order. The full clean-source Editor build on908030e5dc passes, default modules/tests/SCU limit8/no symbols, with a generated actual-input build identity and final artifact SHA2ca93c3a9b70b3895157ee1d71182a81561be5ff38f2b61184296e01914661c1. Focused B4/E25/K03 passes54/1156; native device-exit/Graph/RenderingDevice passes167/3060; final renderer fixture passes Metal Mobile/Forward+. Ordinary MultiMesh public fixtures pass19 matrix and5 motion rows. Evidence: work/b4-e25-full-editor-20261030/RECEIPT.md. This closes coherent Editor/SCU provenance and runtime gaps for those constituents; release/platform/product acceptance is still not inferred.

K02 P1 continuation identified motion preflight rejection retaining old draw resources (size/cache failure branches). An independently reviewed candidate routes them into the existing invalidation/notification outcome. Its controlled four-branch Vulkan test and exact RED candidate are now undergoing a full-source incremental build on the coherent host. No failure test or new repair is claimed passing before execution. Separate command CPU staging-allocation handling and actual same-frame invalidation/draw eligibility remain to inspect.


## K02 motion preflight executed — 2026-10-30

Commitda7dbdbe03ffcaa7e9f76364f5b59d6e0d7ef727 routes all motion preflight failures through the existing resource invalidation and dependency notification. Exact same bounded Vulkan control has RED11 failures/48 assertions and GREEN48/48; full focused helper/GPU suite3/78 and broader regression222/4264 pass. Actual Metal resource matrix19 rows and motion5 rows remain healthy. Full-source incremental SCU builds, copied fixed binary hashes, exact patches, generated build identities and independent review are recorded in work/k02-motion-preflight-20261030/RECEIPT.md. No-test production syntax also passes, with the test-only allocation-result injection compiled out.

The next independent K02 task is actual MultiMesh base dependency registration in both renderer geometry trackers. Existing code registers Mesh surface dependencies while SceneCull separately tracks the MultiMesh. Normal visible-instance and capacity-change fixture controls are being prepared; no unsafe draw or resource failure is needed to observe the missing propagation. This separate dependency gap is not considered fixed by the motion cleanup.


## K02 renderer dependency completed constituent — 2026-10-30

Commitdbded09c646debacb2060a143324890cc9578e84 registers MultiMesh base dependencies in both renderer geometry trackers before empty-state branches. A genuine normal-resource Metal fixture now covers visible1/0/-1, resize3→5, empty-buffer and empty-mesh restoration, and retained Mesh dependency. The initially uninitialized fixture and compiler dead_strip export failure are recorded as failed infrastructure/validation, not accepted RED. Corrected identical fixture sections yield precise baseline RED on both renderers and full GREEN after only6 production lines. Revised GREEN also passes222/4264 combined regressions. Independent read-only review confirmed both production and real SceneCull initialization correction.

Final committed-source rebuild is clean before/after and rc0; fixed binary SHA9e6e833f3e8811190442d24e7edd25b859e033bbdd7f05f99f90b6ce05e99f56 has matching generated artifact receipt and dbded banner, passes Metal Mobile/Forward+ final fixture. Verified source bundle/ref pin the whole local series on7a64. See work/k02-renderer-dependency-20261030/RECEIPT.md. This normal dependency constituent does not claim failed-upload actual no-draw, pixel correctness, physical return or full K02/E12 acceptance. Command CPU staging-allocation handling and the remaining original production-owner/qualification gates remain in scope.


## Command staging result and own artifact recovery — 2026-10-30

Commit77d9037c966009c7dbce8ae724e8773aba33b0f9 now checks command staging Vector allocation before writes. Same existing real Metal resource controls pass19+5; independent source review accepts the early-return/invalid-command ordering, with no actual OOM/failure-draw/physical-return claim. Source and evidence: work/k02-command-staging-20261030/RECEIPT.md.

Under existing user authorization and the upstream cleanup request, removed only13 own superseded old executables (3,965,472,768 allocated bytes) after exact hashes, successor mapping and no-open-file check. Critical original bounds/material/aggregate RED hosts, final reusable Editor, all source/commands/logs and current build artifacts remain. Retired raw paths are explicitly recorded unavailable; historical receipts are not relabeled as current-file existence. See work/obsolete-build-cleanup-20261030/RECEIPT.md and CLEANUP_MANIFEST.json.

E12 create intake audit confirms six APIs/storage owner are not implemented; exact pure schema constraints exist but selected pass bit values and some Indexed144 material/field conditions remain unspecified in inspected normative intake. Actual FinalOutputSource/U04 selection/E20 retained material/U08 reservation/native E09 record port also remain unconnected. No numeric defaults, source grants or qualification proc substitution is added. Continue native local source and original K02 pixel-neighbor acceptance independently.


## Ordinary native color and indirect consumption — 2026-10-30

Commitf4c0aaa368 adds actual ordinary Mesh/MultiMesh/GPUParticles pixel-neighbor controls; Metal Mobile and Forward+ each pass4stage/116fixedsamples. Commitd204d6f3df adds actual publicRS scene/MM indirect consumer controls: same-count direct↔indirect, valid command instanceCount0/3 while logicalcapacity3/visible-1 remains, visible0/-1 and indirect3→5; each renderer passes9stage/261fixedsamples and rawpatchErrors0. Independent saved-PNG oracle verifies232+522 samples across8+18 images. Source and binary inputs stable, no diagnostic/leak accepted, independent source review recorded. These are ordinary native color constituents, not depth/shadow/performance/physical/E12 or fullM12 signoff. Evidence: work/k02-native-neighbors-20261030 and work/k02-native-indirect-20261030.

Next original native texture task received from B/root: E22CurveTexture snapshot after Curve bake_resolution8→2 preserves source/plan identity while sample changes0.944606→0. Source/binary/contract relationships and stale checks must be verified, and snapshot scratch claims audited against actual sample_baked allocations. Exact incoming evidence lives at /Users/macbookair/Documents/Work/ANGE_w1/work/e22-texture-revision-20261030/{RECEIPT.json,RUN.log,texture_revision_probe.cpp,REVIEW.md}. B continues upper-layer consumption and does not change Godot. This native package is next, without touching pending original issuer/Ready/G1 authority.


## E22 native CurveTexture revision and scratch — 2026-10-30

Recovered prior committed candidate from Git after the restart; new source/build/results now live in persistent directories. Commits0a24c5342a and0ea67dec88 repair actual native resolution revision, shared caller-scratch sampling/cache isolation, no-signal writer bookkeeping and native failure result semantics. Fresh RED20cases12fail, final GREEN20/16165 plus ResourceSnapshot14/185, Curve6/89 and Gradient3/22. Clean committed Editor4b02bf7a… and both actual Metal renderer oracles54cases/75665checks pass. Exact evidence and review: work/e22-native-20261030/RECEIPT.md. No E20/U08/Ready/G1 claim. Double-real engine and isolated matching SDK builds are in progress; remaining original E22 and all authorized native FR work stays queued.


## Fixed native checkpoint and continuing queue

Native02d254c4 is committed, clean-built single/double, public on the correct personal fork. Each precision has22/49616 E22 plus original resource tests, both Metal renderers54/75665 image comparisons. Mobile double current draw has an actual cache-disabled RED→GREEN; previous motion remains static-only. Hosted02d static checks pass, Linux compile fails before tests. Independently reviewed fixture-only92796845 fixes observed SCU/GCC diagnostics and is pushed; hosted41072646811 is in progress, not a whole-queue blocker. HANDOFF_A pins exact source/Editor/SDK/ABI bytes. Original E22 owner-order conflict is audited; provisional original-bound→claim→active→native-plan route requires original owner services/contract amendment. Gradient invalid-enum refusal is under native RED testing. No global completion/Ready/G1 claim.


## Gradient profile and remaining normal native work — 2026-10-31

Public2919f143 fixes an actual undeclared-enum fallback before native sorting/writes. Exact fixture RED4cases1fail/18711 and same-fixture single/double GREEN4/18711; E22 totals26/68327 plus ResourceSnapshot14/185. Committed source is clean-built in both precisions. Receipt work/e22-gradient-profile-20261031. Hosted927 status success includes four String append nonnull diagnostics; do not label it sanitizer-clean. Hosted2919 run41085218611 is live. Next independent work: ordinary empty String/Span append semantics/source guard; prepared Gradient GPU270-combination oracle with actual single/double SDK/API receipts; original owner order/scratch-cell lifecycle/failure prefix require coordinated contract decisions. Local/CI waits do not complete the remaining full Godot/typed-close queue.


## Continued ordinary native work — 2026-10-31

Public2952fcfc includes the reviewed ordinary String empty-span guard and repository Gradient GPU fixture. Clean single/double builds and exact-checkpoint resource tests pass; Gradient native GPU270/372870/0 on each of four paths. New receipts: work/e22-gradient-gpu-20261031 and work/string-empty-append-20261031. Hosted2919 passed jobs with four pre-guard String diagnostics; new2952 CI41092146657 pending. Next independent approved native lane is original B4/M12 ordinary depth/shadow neighbors, beyond already recorded color constituents. This is ordinary rendering development only; no security work, no scheduled task, no G1/whole-queue completion.


## Ordinary depth/shadow constituent — 2026-10-31

Local `cbd9e19a3fd5` adds approved M12 ordinary depth/shadow neighbors. Four healthy precision/renderer paths each6stages/216samples/0; eight public-flag controls each exactly18 target failures and healthy neighbors.72PNG/2592independently verified samples, independent review, style pass. Runtime engines are2952, not relabeled as the new fixture commit. See work/k02-native-depth-shadow-20261031/RECEIPT.md. Exact-commit optimized template_release single build is in progress; public2952CI remains its own lane. Full M12/B4/owner/long-goal gates remain active.


Public2952 CI41092146657 completed success; full retained Linux log has0checked runtime diagnostics, with all four focused selectors nonzero and passed. Both exactcbd9 template_release builds also completed rc0/source-stable (single428f08d8…,double5537e7cd…); actual packaged depth/shadow and native-simulation acceptance is in progress. Original --path template invocation failed before project load; standard main-scene PCK entry is used for actual release execution. No old failure is relabeled passed.


## Continued normal native simulation and release rendering — 2026-10-31

Public source1cfcfcf021ca adds fixtures/PCK helper only; native engine source is unchanged from2952. Bothcbd9 clean optimized release-template builds pass. Editor/release planar simulation40runs each4stages/288samples:32healthy0failed,8omission controls exact18target failures, allwrappers0;160PNG/11520samples independently decoded and reviewer checked. Release depth/shadow12runs,72PNG/2592samples also pass with exact18sample publicflag controls. Exact engine/fixture/package identities and excluded exploratory failures remain separate in work/k02-native-simulation-20261031 and work/k02-native-depth-shadow-20261031. Review/style pass; source push verified. Latest public CI will be separately recorded. Full M12/B4 and original owner/typed-close/G1 remain active obligations; these local results do not complete the long goal.


## Linux native resource coverage and CI source grouping — 2026-10-31

Publicdfad44ca7b adds existing E22/resource/String CPU selectors and an explicit SCU limit8 to the personal fork job. The cache worked; this scopes invalidation of large test source groups. Actual generator proofs preserve224 test sources and1516 overall generated source entries exactly. Independent review/style/local selector checks pass. Hosted 41102480721 is queued; true Linux grouping/link/runtime and performance remain pending. Parent1cfc hosted41099309233 completed success; direct complete job logs show four nonzero selectors and0checked diagnostics, separate from the new seven-selector profile. Evidence work/native-ci-cache-20261031/RECEIPT.md. Long goal stays active; no whole-family/owner/typed-close/G1 completion follows.


## Test build reproducibility — 2026-10-31

Local141676c2cd canonicalizes force-link source discovery order. Exact-fixture RED/GREEN2tests, actual219-source generator corpus and independent review pass. Clean native Editor build/catalog comparison is underway; Linux/new-CI results remain separate. Evidence work/force-link-order-20261031/RECEIPT.md. Long goal remains active.


Local141 native build/catalog validation finished: clean single Editor rc0/stable, original1904case multiset and printed order preserved, E22 26/resource14/String99 all pass. Publicdfad CI41102480721 is live; local141 is retained pending that result. No new GPU/Linux or full-family qualification is inferred.


Local141 resource-neighbor follow-up: original-bin Image7cases rc0 after verifying identical artifact SHA and expected executable-relative test assets. Relocated-copy Image invocation failed at missing BMP REQUIRE and subsequentlySIGSEGV; retained/excluded, not represented as an engine fix. Curve/Gradient extra logs report9/5passed. Publicdfad CI remains live after one transient HTTP502 observation; no restart or terminal inference was made from that polling error.


## E22 CPU Image original-source reception — 2026-10-31

Read-only audited Godot141 and A ecc5 current reader interfaces and12actual source snapshots, independently reviewed. Real reader/fixture source exists; original binding/version begin/end, revocation, complete slice mutation gate and permitted-thread owner declaration are not supplied by those reader/Image APIs. Native revision export accepts Curve/Gradient only; no Image/Ref/COW/metadata identity is substituted. B reader/cell bridge can proceed independently, with original owner imports as a separate live admission requirement. See work/e22-cpu-image-source-20261031/RECEIPT.md and FINDINGS.md. No new runtime result or full E22 qualification is claimed.


Publicdfad hosted41102480721 now completed success. Complete direct log verifies all seven selector groups (including E22 26/68327,resource14/185,String99/1462) and0checked diagnostics; SCU8 Linux compilation/link is no longer pending for this commit. Other OS/GPU/whole-family gates remain separate. Local141 force-link change can now fast-forward without cancelling that completed run; actual push/newCI recorded separately.


Public141676c2cd force-link determinism fix fast-forward pushed afterdfad CI completion, remote verified. New CI41107207922 in_progress is its own pending check; prior141 local1904catalog and resource results remain scoped. The full native/typed-close goal remains active.


Local64874c00b6 adds build-generator Python regression execution to the fork CI profile, after successful local2+21tests/style. It is held while141 CI41107207922 is live; no new hosted result is inferred. This closes the missing future CI entry for those test files once synchronized and verified, not whole native/typed-close acceptance.


Public141 hosted41107207922 completed success: seven full-log selector groups passed with0checked diagnostics; no Linux/GPU/global-init overclaim. Local CI-entry successor64874c00b6 is now fast-forward synchronized and remote verified. New CI41108371898 in_progress must independently run the2+21Python tests and seven native groups. Original owner/whole-FR/typed-close obligations remain open.


## C04 original public numeric runtime and CI correction — 2026-10-31

Real local Godot Metal execution validates eight immutable original emitted terminals across240admitted public rows:127560uint32 final words/510240bytes match the original public table, independently decoded and reviewed. One active terminal per64lane group, real local submit/sync and full buffer readback; original prefix unchanged. Initial missing-sampler host failure is retained; complete corrected and strengthened-stability-gate rerun passes. No sealed/target data was read. Evidence work/c04-original-glsl-native-20261031/RECEIPT.md. This closes only this public numeric runtime constituent, not fullC04/G2/entry/owner/G1/typed-close or the queue.

Latest hosted648run41108371898 succeeds with actual2+21Python tests and seven native groups. Complete log contains two emitted driver_callback_add diagnostics. Re-read of retaineddfad/141 logs found the same two lines: earlier zero-diagnostic summaries are explicitly superseded by per-run DIAGNOSTIC_CORRECTION.json addenda. Test counts remain passed; diagnostic-free qualification is withdrawn, no source or suppression change made. Continue ordinary native work independently; long goal remains active.


## Ordinary native MultiMesh cross-frame history — 2026-10-31

Source4a9042a03fec15957989502659022cd75361eef3 adds interpreted Forward+/Metal native velocity sampling and strict renderer/precision/identity/control runner only. Four real single/double Editor/release hosts × normal/control all produce expected results: normal114/0, omission controls114/10 exactly9velocity+1color. Actual48RGF/48PNG independently reviewed;864velocity pairs and48colors agree, this saved matrix393216float components all finite. Source/worktree clean, project hooks pass, bundled source verifies, ordinary fast-forward push/remote verified. PublicCI41113063502 is in_progress, not passed. Evidence work/k02-native-motion-history-20261031/RECEIPT.md. FullM09/K02/B4, otherrenderer/material/camera/history and owner/typed-close/G1 gates remain open. No security or invalid-resource experiment performed. Long goal stays active.


## Native cached MultiMesh motion repair — 2026-10-31

Ordinary per-instance cached transforms produced false initial/resize velocities from zero previous matrices. Native5a815b9578 fixes first-upload history and tracks only actual successful nonempty updates; fixture successor4a2f5b8b4e fixes explicit full-geometry bounds. Same final fixture old141 RED cached/start-hidden/hidden-only-first18fail each, late-motion9atresize; original late-motion initial is a preserved healthy neighbor. Four clean source-stable Editor/release single/double builds,40actual Forward+/Metal host profiles,240RGF/240PNG,4320velocity pairs and4original healthy resource neighbors now pass their scoped controls. Non-author source+data review passes after the sparse-zero-upload finding was addressed. Both commits normally fast-forward pushed/remote verified; new hostedCI remains separate. See work/k02-native-cached-motion-20261031/RECEIPT.md.

Next normal native gap: test distinct hidden-instance edits on separate frames followed by direct reveal without rewriting transforms. Current start-hidden deliberately rewrites all transforms before reveal and does not cover that carry path. Continue this ordinary cached motion/visibility line independently of original owner/typed-close external dependencies. FullM09/K02/B4/E34/owner/G1 remain open.

Prior4a904 hosted41113063502 completed success with actual2+21Python tests and seven native groups; its two actual callback diagnostics are retained, so no diagnostic-free qualification is inferred. Public4a2 candidate hosted execution is separately pending.

Public4a2f5b8b4e CI41118033192 now completed success: actual2+21Python tests and seven native groups all passed from complete direct log. Two actual callback diagnostics are retained, so no diagnostic-free or GPU/platform-wide claim follows. Cached-motion repair package has source/review/four clean builds/40GPU runs/normal push/hosted smoke; wholeM09/K02/B4/typed-close/G1 and remaining hidden multi-frame direct-reveal behavior stay queued.


## Hidden cached instance propagation — 2026-10-31

Native04a93980a2 with frozen fixture1f33b64355 fixes hidden per-instance edits lost across motion CPU-cache swaps. Only CPU prior dirty-region carry now visits all allocated regions; GPU visibility/upload code is unchanged. Six exact-fixture baselines expose motion18fail, motion-off0 and omission36 for stride12/20. Four clean source-stable same04builds and24actual Metal host profiles now satisfy full CPU records, direct-reveal complete GPU data and6480pixel samples, including1025partialtail/color/custom; omission is exactlyB18 withAretained. Sixteen original fresh/late-motion velocity host neighbors also pass exact controls. Non-author source and full saved-data review pass. Normal fast-forward push and remote04a head verified; hostedCI separately pending. Evidence work/k02-hidden-cache-carry-20261031/RECEIPT.md. WholeM09/K02/B4/E34/physical/owner/typed-close/G1 stay open.

Next integration intake must use fresh inputs: read-only Git metadata now observes A-mainb63a46edc28ade4a65dc298137d8a7940df1190b and A-combo99f097036bc971515df367e965626bcc7eeabc62, advanced from the older reception. This is not a loaded-DSO/source join or owner-readiness assertion. Inspect their latest fixed binary/build/source and original supplied ports before reusing any historical owner-unavailable conclusion. NEXT_INTEGRATION_INPUT_SNAPSHOT.json records exact limited observation. Continue original native and current-pair integration within existing authorization.


## Current pair, actual SDK inputs and color-use source reception — 2026-10-31

Public04a CI41123984753 completed success, actual2+21Python tests and7native groups all passed from full direct logs. Two callback diagnostics remain recorded; no diagnostic-free qualification follows. Hidden-cache receipt and manifest now include that terminal CI evidence.

Fresh byte-frozen ANGEf6ff22c6 with fixed04a single Editor ran ordinary non-headless Metal Mobile no-VFX intake: rc2, LOAD_STATUS_FAILED, original E05 host.storage_owner_transport refusal before EcsWorld entry/unload. No loaded DSO/source join is established. Separately, real04a API export and clean isolated62b SDK build succeeded; exact core selection and archive/binding stamp are delivered as build inputs, not an Adapter link or owner/Ready receipt. See work/current-pair-intake-20261031/RECEIPT.md and HANDOFF.md. Matching double SDK build is in progress, not yet passed.

Fixed04a E14 source reception records existing camera tokens/adoption and identity-only NativeRetirementReceipt, including absence of production issue/ack callers. No actual ANGE pending-packet to particular color use/list-end/last-use join is demonstrated. Contract bytes and16source inputs are pinned/rechecked; independent-context review covers the read-only audit, no runtime/ABI completion claim. See work/e14-color-adoption-source-20261031/RECEIPT.md. Original association/owner/typed-close/G1 gates remain open; all authorized ordinary Godot work remains the active long goal.


Current-pair build-input follow-up finished: actual04a double API export and clean62b double SDK archive build pass rc0/stable; original helper digest and independent archive review pass. Four ordinary current-frame Metal precision/renderer runs pass with8PNG independently checked, and original4SDK graph tests plus2actual4.7precision selection graphs pass. Dry-run graph selection, normal shader drawing and static archive receipts are distinct from Adapter link/owner/operation/completion. No cleanup was performed; current free space is sufficient. Original U08 retained binding and E09/K04 packet-use joins remain scoped open; goal and remaining Godot queue stay active.


## Native M10 layout and actual canvas drawing — 2026-10-31

Public26749683a0 adds fixture/runner/docs only; successor7599029534 changes only the runner executable mode after hosted267 static hook failure. All final source bytes still match the frozen matrix. Four original clean04a Editor/release single/double binaries executed16actual Metal Mobile/Forward+ normal/control cells:4320primary and864fresh-target-marker samples, complete240byte original3D and64byte2D records plus native command words.8normal hosts pass,8omission hosts fail only their36specified pixels each; all wrappers pass. Independent offline review decodes96PNG,224CPU/GPU records,112commands and8PCKs. Full exact sources, exploratory exclusions, style/CI diagnosis, patches, verified bundle, normal push and remote759 head are retained in work/k02-native-layout-canvas-20261031. Canvas direct draw is not indirect-command consumption; fixed bounds do not qualify automatic bounds. WholeM12/B4/E34 and product owner/operation/completion/typed-close/G1 remain open; long goal stays active. Successor hostedCI is separately pending until terminal full-log verification.

Next original M12 work is current-engine neighbor integration: the full depth/shadow and native-simulation packages remain assigned to earlier2952/cbd9 engines. Their saved success must not be relabeled as04a after the fresh-cache and hidden-cache production repairs. Reuse the existing original fixtures and all normal/negative profiles on the four fixed04a hosts (Editor/release, single/double, Mobile/Forward+) without inventing owner or External-path receipts. This is the existing M12 neighbor requirement, independent of the original owner blockers.


Public7599029534 hosted41134479049 is now terminal success. Complete direct job log verifies2+21Python tests and seven nonzero native groups, all0failed; static checks pass. Six Python DTD-fixture warnings, four build msgfmt fallbacks and two native callback diagnostics remain recorded, so no diagnostic-free or GPU/platform-wide qualification follows. This supersedes only the earlier successor-pending snapshot. The new canvas constituent and current-engine M12 neighbor continuation remain separately scoped; long goal stays active.


## Current compiled M12 neighbor evidence — 2026-10-31

The existing original five-source snapshot ran64real Metal cases on the four fixed clean04a binaries after the cache production repairs:40simulation profiles and24depth/shadow controls,304PNG/16704samples and32actual release PCKs. Forty healthy hosts return0;24controls return1/exact18specified pixel failures each; all wrappers0 and independent raw-log/image/PCK checks pass. Evidence work/k02-current-m12-neighbors-20261031/RECEIPT.md supersedes only the current-engine-neighbor pending status for these original profiles. Global-frame waits, sampled depth colors, target attribution and other limits are explicit; fullM12/B4/E34/owner/typed-close/G1 remain open. No new source or CI result is implied.

Original QRT sequence read-only reception separately pins7source files and4Git objects. Same v1 activate symbol differs between60193 and890/04a/759; retained-cell reactivation is narrowly gated and is not release/reuse or physical completion. Caller operation watermark and native child cleanup credit are distinct. Current A source prerequisites are recorded without a runtime create-at-MAX or complete-budget claim. See work/e05-original-serial-source-20261031/RECEIPT.md and independently reviewed FINDINGS.md. No retry optimization, ABI change or previously interrupted experiment was enabled. Original A/same-session/budget obligations remain scoped open; long goal stays active.


Current M12 follow-through still has a concrete evidence gap: the unchanged original fixtures use global frame waits and do not emit unique per-target stage markers or explicit runtime target-feature observations. The next ordinary fixture implementation should add those observations while preserving every existing simulation/depth/shadow profile and literal neighbor oracle, then validate the resulting source and complete matrix. Current64saved-image results remain scoped and must not be promoted to distinct-target-frame or fullM12 completion.

E05 create-credit follow-up: source audit of the original Godot create/RD/QRT chain and frozen A consumer finds that a complete state2/non-OK/zero-backing delivery plus successful release does not by itself prove a pre-driver no-effect refusal. The null-BufferID branch can emit the same tuple after `driver->buffer_create` was called. Release retires the QRT operation watermark and frees occupancy; it never recycles the issued caller serial. A proved no-effect result may remove only that part's future backing/first-retire obligation. Original same-session stage/rollback proof and all-consumer near-limit budget closure remain open; no retry/credit implementation was made. Evidence and exact scope: work/e05-original-serial-source-20261031/CREATE_CREDIT_CONTRACT.md and updated RECEIPT.md. Continue independent ordinary Godot work; do not promote this to Ready/G1.

The existing single-precision Editor also passed three narrow E05 synthetic Registry/driver controls (65/65 assertions) on the exact retained binary. This supports the already implemented event-capacity and QRT retirement controls only; it supplies no actual A/D session or backend rollback proof. The full native/typed-close queue remains active.


## Current compiled M12 stage witness follow-through — 2026-10-31

Godot local commit `df6b466169bc1bb4574c536f028463b391e642be` adds target-image stage markers and runtime host feature observations to the four ordinary simulation/depth-shadow fixture/runner files. On the four fixed04a compiled engines, all64 Metal Editor/release, single/double, Mobile/Forward+ profile rows passed their wrappers. Offline recheck, repeated after commit, decoded304 PNG,16,704 original primary pixels,2,736 new marker pixels and32 four-entry release PCKs;24 original omission rows retained exact18 target primary failures each. A private last-marker omission produced zero primary failures and exactly9 marker failures, with wrapper rejection. Package `work/k02-m12-stage-witness-20261031/RECEIPT.md`, matrix, case index and manifest retain exact limits. An attempted independent final code review did not return a verdict, so review remains pending. The marker proves sampled target-image overlay freshness only; full M12/B4/E34/owner/typed-close/Ready/G1 remain open, and the long goal stays active.

The isolated `df6b466169...` commit was fast-forward pushed to personal `origin/codex/particle-mainline` after `git ls-remote` showed its parent `7599029534...`; the remote was re-read at the exact new commit. A README scope follow-up `4391c5307883dd1a2649e7ff60cc5512c561070f` was then fast-forward pushed and remote-verified. The primary local checkout remains divergent at `890853e917...` with prior changes and was untouched. Hosted run41147901189 ended cancelled after its static job succeeded; successor run41148205462 is in progress. No whole-run CI or final independent review result is claimed.

E05 whole-result continuation: Godot commit `8e585330107467c6488e4edd88a1d18c6e0f7358` adds a real Registry/QRT kind-2 test with a synthetic driver. Pre-driver recording refusal and post-driver empty BufferID produce the same state2/FAILED/reason16/72 zero bytes and successful release; driver invocation differs 0/1, and released serial1 cannot be re-reserved. A temporary driver-call-bypass mutation makes the test fail, restored final single/double `[Qualification]*` groups each pass92/2010. This strengthens the source counterexample only; original loaded A/D same-session, backend rollback and full caller budget remain open. Evidence: `work/e05-qrt-create-tuple-20261031/RECEIPT.md`. The commit was fast-forward pushed to personal origin and read back. Hosted run41148205462 for predecessor4391 is now terminal success, static plus Linux sanitizer smoke with seven nonzero native groups; see retained full log. This does not resolve independent M12 review or full M12/typed-close/Ready/G1. The long goal stays active.

The existing actual Vulkan staged buffer-create rollback test was rerun on the final single Editor and passed1/8. It covers the two injected extra-tracking Vulkan failure stages and unchanged VMA allocation count, without joining the original QRT/A session or proving Metal/D3D12/general allocator rollback. Its log and scope are appended to `work/e05-qrt-create-tuple-20261031/RECEIPT.md`.

M12 fixed-scope final review has since arrived from a non-author coordinator. It examined `7599029534...4391c53078`, reran the saved verifier and 816 case-file hashes, and found no P1/P2 defect in the target-image marker fixture. `work/k02-m12-stage-witness-20261031/INDEPENDENT_FINAL_REVIEW.md` and its updated receipt/manifest retain the verdict and the terminal successor CI result. This closes the previously pending fixture review, not complete M12/GPU/owner/typed-close acceptance.

E05 commit `8e585330...` hosted run `41149260209` is also terminal success. The full direct job log is retained in `work/e05-qrt-create-tuple-20261031/`: static and Fork Linux sanitizer smoke jobs succeeded; seven nonzero native groups all passed with zero failures. Other platform jobs were skipped. This closes the CI pending state only for the exact test commit, not the original A/D session, backend generality, whole serial budget, or product gates.

`work/e05-qrt-create-tuple-20261031/BACKEND_CREATE_BOUNDARY.md` now pins ordinary source-level failure-prefix paths for Metal direct/heap, Vulkan VMA/extra-tracking and D3D12 `ComPtr` create/map. The real Vulkan two-stage injection is the only backend failure-path runtime in this continuation. Metal and D3D12 remain source-only, and none establishes an original loaded A/D same-session no-effect credit. This is a concrete backend evidence queue, not a cross-backend rollback claim.

Godot commit `de22b7fa8d9c73d7210869a78507a7251259ddf5` adds the next E05 caller-limit control: one published backing survives a failed create at `UINT64_MAX-1` in both pre-/post-driver arms, stale serial reuse is rejected, and final serial `UINT64_MAX` executes original kind-3 retirement to physical fixture state1. Temporary retired-watermark mutation failed at stale reuse; restored single/double Qualification groups each pass93/2217. The clean committed source was fast-forward pushed to personal origin and remote-verified. Evidence and exact limitations: `work/e05-caller-limit-20261031/RECEIPT.md`. This is a one-backing synthetic Registry/QRT boundary, not A's original all-consumer/multipart/pending budget or loaded same-session authority; hosted CI for this commit remains pending independently.

Godot commit `ef5be520795d95f7ed072003594520ceeaecd096` expands the same actual QRT control to two distinct published backing parts. A failed create at `MAX-2` is followed by successful original retirement dispatches at `MAX-1` and `MAX`; both report state1 and are freed. Single/double Qualification groups each pass94/2460, and personal origin was fast-forward pushed and read back at the exact commit. Evidence: `work/e05-two-parts-20261031/RECEIPT.md`. This closes only the bounded two-part synthetic Registry budget branch; pending/Unknown, other same-binding consumers, original A/D same-session and backend generality remain open. Hosted CI for this new commit is independently pending.

Godot commit `823ce724bcd309c92569338cfa6436a43455583a` adds a test-only Metal heap buffer failure after real heap allocation and before `newBuffer`, checking original `_free_allocation` rollback via three-pool live stats and a subsequent successful buffer. Single/double Metal-plus-Qualification groups each pass97/2525. Personal origin was fast-forward pushed and remote-verified. Evidence: `work/e05-metal-heap-rollback-20261031/RECEIPT.md`. This is a specific injected Godot heap-prefix branch, not a real Metal API failure or all-backend no-effect proof. A production root still rejects `host.storage_owner_transport` because the authentic U08 no-world issuer input is not available; no fake grant was added. The prior ef5 CI41150645627 reached success, intermediate de22 CI was cancelled after successor push, and new823 hosted CI remains separate.

E05 hosted formatting follow-through: original Godot commit `823ce724...` run `41151681931` failed static hooks (`clang-format`, `copyright-headers`); Linux smoke was skipped. Successor `f7c5288609` formats the Metal qualification test under pinned clang-format 21.1.7, adds its required Godot header, and was fast-forward pushed/read back on the personal branch. Second formatting pass and `git diff --check` were clean; single/double incremental Editor builds and focused Metal-plus-Qualification groups each passed 97/2525 with the two existing callback diagnostics. Hosted successor run `41152179808` is in progress, so no hosted success or full E05/A-D/typed-close/Ready/G1 claim follows. Evidence: `work/e05-metal-heap-rollback-20261031/RECEIPT.md`. The long Godot goal remains active.
The E05 backend create-prefix source audit has also been updated at `work/e05-qrt-create-tuple-20261031/BACKEND_CREATE_BOUNDARY.md`: actual Metal heap-context injected rollback is recorded as one bounded branch; actual Metal API failure and original loaded A/D session remain unproved.
Successor `f7c5288609` hosted run `41152179808` subsequently reached terminal success on exact head: static hooks passed, Linux smoke passed 2+21 Python tests and seven nonzero native groups with zero failed assertions. Full direct log retained in the E05 Metal heap package. Two callback diagnostics and first-group queue-memory messages remain in the log, and other platform jobs were skipped. This supersedes the earlier in-progress line only; Metal actual API failure, original A/D same-session, typed-close, Ready and G1 remain open.

E05 dedicated Metal heap follow-up: public `7111cd8ed3` adds a test-only actual-context 65 MiB dedicated branch after allocation/before `newBuffer`. A temporary cleanup-bypass mutant fails exactly allocation validity plus dedicated count/bytes; restored final single/double Editor Metal-plus-Qualification groups each pass 98/2542. Personal remote exact HEAD verified; hosted `41153055104` is in progress. See `work/e05-metal-dedicated-20261031/RECEIPT.md`. Actual Metal API failure, original A/D session and product gates remain open.

E05 shared-pool local follow-up: `8c2369f5f8` adds actual Metal-context Shared and WriteCombined injected failure controls; a temporary non-private cleanup bypass fails both arms (six assertions), restored final single/double groups each pass 99/2569. Predecessor hosted run remains in progress, so this successor remains local pending push sequencing. Evidence: `work/e05-metal-shared-20261031/RECEIPT.md`; original A/D and product gates stay open.
Dedicated-heap predecessor `7111cd8ed3` hosted run `41153055104` is terminal success on exact head: static plus Linux sanitizer smoke passed 2+21 Python and seven nonzero native groups. Full log retained; two callback diagnostics, first-group queue-memory messages and skipped other-platform jobs limit the claim. Shared-pool successor may now be pushed without cancelling this result.
Shared-pool successor `8c2369f5f8` was fast-forward pushed and exact remote HEAD verified after predecessor `7111cd8ed3` hosted success. Its own hosted run `41153610387` is in progress; local single/double 99/2569 and mutation evidence remain the current scoped proof.
Shared-pool `8c2369f5f8` hosted run `41153610387` is now terminal success on exact head: static plus Linux sanitizer smoke passed 2+21 Python tests and seven nonzero native groups with zero failed assertions. Full direct log retained; two callback diagnostics, first-group queue-memory messages and skipped other-platform jobs limit the claim. Original Metal API failure, A/D same-session, typed-close, Ready and G1 remain open.
Space maintenance under prior user authorization: after all E05 final builds and three terminal hosted CI runs, removed 2,021 reproducible double `.o`/`.a` files (968,481,336 logical bytes) from the isolated native worktree only. Tested executables, sources, logs and receipts remain; disk free rose 3.4→4.3 GiB. Future double work requires object regeneration.

B4/E18 normal native follow-up: `2e8b2d14cc` rejects saturated `UINT64_MAX` material generation when issuing identity-only `NativeRetirementReceipt`, matching existing material admission policy. Focused pre-fix test failed 1/2 assertions; final single/double combined B4+RenderingDevice groups each passed 51/872, with original negative-control diagnostics retained. Personal remote exact HEAD verified; hosted run `41154845664` queued. Evidence: `work/b4-saturated-retirement-20261031/RECEIPT.md`. No production issue/ack, per-output packet, color adoption, GPU completion or product gate is claimed.
B4 saturation fix `2e8b2d14cc` hosted run `41154845664` is terminal success on exact head: static plus Linux sanitizer smoke passed 2+21 Python and seven nonzero native groups. Full log retained with two callback diagnostics and first-group queue-memory messages; other platforms skipped. This closes only the narrow receipt-issuance sentinel mismatch, not B4/E14/E18 production association or completion.

R01 native frame-source error follow-through: Godot commit `30548f251c` closes the actual failing frame and pending readback on chain or hidden native source error. Original code failed the focused poll/retry test; the first repair failed an older-frame result check. Final single/double builds and 168/3996 related tests per precision passed. The personal remote was fast-forward pushed and exact head verified. Evidence `work/r01-native-frame-error-20261031/RECEIPT.md`. Double build first hit `errno=28`; under prior authorization 3,457 reproducible single-precision `.o/.a` files in this isolated worktree were cleaned, then double build passed. This is a frame fail-closed repair, not production original-operation/R02 source association, owner ack, full typed-close, Ready or G1. Long goal stays active; continue independent Godot FR work.

Exact successor `e5ed473e3c` hosted run `41162180496` is terminal success: static checks, 2+21 Python tests and seven native groups totaling 271 cases/72,979 assertions pass. Full direct log is retained in `work/r01-fence-frame-error-20261031/`; existing callback diagnostics and skipped other-platform jobs remain explicit. Local double 156/3937 remains the scoped native result; the older single executable is not relabeled. Next ordinary K02 work extends the existing real main-RD matrix for multi-surface visibility/capacity/layout and empty-mesh recovery; original owner/typed-close/G1 obligations remain open and the long goal stays active.

## K02 mixed-surface command coverage - 2026-10-31

Public `d8c9643eea` extends the original main-RD matrix, preserving19 rows and adding11: mixed indexed/nonindexed command words, visible1/0/-1, capacity7 with3D/color/custom, 2D capacity2, null/empty recovery. Actual double Editor Metal Mobile and Forward+ each pass30/30. Each stale-second-surface control exits1 with only its intended visible-count row failing; original Forward+ motion neighbor remains5/5. Persistent verifier checks complete row identities/word literals, exact requested renderer, host exits/signals and input stability. Independent source/evidence review has no blocking findings. Normal fast-forward push/remote head verified; hosted41163820993 is queued. Evidence: `work/k02-multi-surface-matrix-20261031/RECEIPT.md`. No production C++ change, physical retirement, indirect drawing or fullK02/M12/B4/typed-close/G1 claim. Continue original M11/native failure and owner integration obligations independently; long goal remains active.


## E05/A-D retained-cell seam check — 2026-10-31

Native D source audit confirms the exact retry boundary in `core/extension/gdextension_qualification_rt.cpp`: after an observed state-2 non-OK result, `qualification_rt_activate` may reuse the same retained QRT cell only when the original kind-3/5 child matches and proves no retirement effect (`!creating`, `!retiring`, `!acknowledged`, no physical close/enqueue/free event). It clears the prior result and reschedules the same cell; it does not mint a new child or pretend completion. `qualification_rt_release` rejects queued/executing/unobserved/pending cells and state-3 cells without `physically_retired`; for a cleanup child it separately checks `cleanup_serial_reserved` and either acknowledged/delivered or reserves a distinct cleanup serial before releasing the cell. Thus A's Binding operation serial credit and D Registry child/retire/dependency serials are independent; A cannot spend a new serial as D capacity, and D's same-cell retry does not itself return A credit.

Actual current test-enabled macOS Editor run of `[Qualification][E05]*`: 21 cases / 1,044 assertions passed, rc0. This covers last-serial dispatch retry, private-capacity cleanup, conflicting owner wait, cleanup-slot return, prepared-ticket cleanup capacity and related physical release paths. It does not prove A's separate-RT transport repeated-refusal policy or original same-session owner input. A's reviewed candidate remains correct to keep its repeated-refusal backing Retiring until an owner contract chooses same-cell retry or deliberate blocked close.


## Particle AABB/readback size-boundary repair — 2026-10-31

Local Godot candidate in `work/particle-aabb-size-20261031/` hardens RD particle size validation. `_particles_get_data_size()` now uses checked `uint64_t` arithmetic for amount, trail expansion and userdata stride, rejects overflow and sizes above the RenderingDevice buffer limit, and both current AABB and CPU readback consume that result without `int` narrowing. The overflow regression and bounded trail/userdata case pass in a rebuilt macOS Editor: `[ParticlesReadback]*` 2 cases / 7 assertions and `[ParticlesTrail]*` 1 case / 5 assertions, rc0; `git diff --check` passes. The opt-in real Vulkan trail test was not enabled. This is a scoped fail-closed CPU/readback boundary repair and does not claim GPU qualification, owner/source association, physical retirement, Ready, G1 or complete particle-system acceptance.

Remote synchronization note: particle size-boundary patch was cherry-picked onto the remote development tip without conflict and fast-forward pushed as `fb460e28b6`; remote SHA was verified with `git ls-remote`.


## Particle GPU buffer size bounds — 2026-10-31

Commit `6068a3631a` extends the particle fail-closed boundary from CPU readback to RD allocation. Checked `uint64_t` helpers cover trail/userdata particle buffers and transform/instance buffers, including motion-vector doubling and the `uint32_t` RenderingDevice limit. TDD red compile and final renderer/test SCU compiles are recorded in `work/particle-gpu-buffer-size-20261031/RECEIPT.md`; format and diff checks pass. Hosted run `41170695161` was in progress at capture. Runtime executable test and full GPU qualification remain pending; no owner, physical retirement, Ready or G1 claim follows.


Hosted CI `41170695161` for particle GPU buffer bounds commit `6068a3631a` completed successfully: static checks and Fork Linux sanitizer smoke passed; other platform jobs were skipped. The retained Editor fresh `[Qualification][E05]*` recheck also passed 21 cases / 1,044 assertions. These results advance scoped native boundaries only; original owner/session association, backend GPU qualification, typed-close, Ready and G1 remain open.


E05 A wiring contract: `work/e05-original-serial-source-20261031/A_MINIMUM_WIRING_CONTRACT.md` now pins same-bridge/session identity, exact retained ref, state-2 non-OK/no-effect-only reactivation, physical-retirement release rules, and the executable `[Qualification][E05]*` control entry. It explicitly preserves fail-closed behavior and does not grant owner, Ready or G1 status.


## Particle sort-buffer size bound — 2026-10-31

Public `996becbb4d` adds checked sort-buffer sizing for particle view-depth sorting. Odd-count padding, 64-bit multiplication, zero amount, and the RenderingDevice `uint32_t` cap are validated before allocation; invalid sizes fail closed. Focused test and renderer storage SCUs compile with `-Werror`; full editor link/runtime remain pending after errno 28 during unrelated Jolt archive generation in the isolated worktree. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. No GPU qualification, owner/source association, physical retirement, Ready, G1 or complete particle-system claim.
## Particle trail dispatch bound — 2026-10-31

Public `5081916109` adds checked total particle amount handling for trail-expanded process and instance dispatch. Zero/default trail semantics, 64-bit multiplication, and the RenderingDevice `uint32_t` dispatch cap are covered by focused tests; invalid totals fail closed. Targeted test and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.


## Particle emission buffer bound — 2026-10-31

Public `2e050e4b5d` adds checked emission-buffer sizing for the header plus per-entry payload. Zero, arithmetic overflow, and the RenderingDevice `uint32_t` allocation cap fail closed before resize/create. Particle test `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle trail bind-pose allocation bound — 2026-10-31

Public `545c63ef57` adds checked trail bind-pose buffer sizing before storage-buffer creation. Zero, multiplication overflow, and the RenderingDevice `uint32_t` cap fail closed. Particle `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle frame-parameter buffer bound — 2026-10-31

Public `3fd7055213` adds checked frame-parameter sizing for allocation and update paths. Zero, multiplication overflow, and the RenderingDevice `uint32_t` cap fail closed. Particle `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle trail bind-pose upload bound — 2026-10-31

Public `f64a8daa72` reuses checked bind-pose byte sizing for staging resize and GPU buffer update, removing the remaining narrow pose upload multiplication. Renderer storage SCU compiles with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle copy dispatch bound — 2026-10-31

Public `feace42b72` validates trail-expanded total copy dispatch amount before view-dependent instance copying. Renderer storage SCU compiles with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle emission buffer update bound — 2026-10-31

Public `7c050caf64` reuses checked emission sizing for per-frame buffer updates. Renderer storage SCU compiles with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle trail history index bound — 2026-10-31

Public `f249faab9e` adds checked 64-bit trail history sampling before indexing frame history, covering multiplication overflow and invalid zero/out-of-range indices. Particle `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle lifetime split bound — 2026-10-31

Public `cee7e90995` adds checked lifetime split arithmetic for RD particle copy push constants, replacing narrow `int(amount * phase)` conversion and rejecting invalid phase inputs. Particle `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle lifetime phase saturation — 2026-10-31

Public `f60ede51bc` hardens checked lifetime phase conversion against finite but huge doubles before integer conversion. Focused particle and renderer SCU compiles remain green; full editor link/runtime and GPU qualification remain pending.

## Particle trail history sizing bound — 2026-10-31

Public `2cf5e9d142` adds checked trail history sizing before `LocalVector` resize, covering floating-point to integer overflow and invalid FPS/lifetime inputs. Particle `scu_tests_23` and renderer storage SCUs compile with `-Werror`; format and diff checks pass. Receipt: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full editor link/runtime and real GPU qualification remain pending; no owner, physical retirement, Ready, G1 or complete particle-system claim.

## Particle narrow history cap and sort rejection — 2026-10-31

Public `ecd83e5b5c` narrows the history cap to `INT_MAX`, matching its consumer. Hosted `41175494600` passed static checks, 2+21 Python tests, and seven existing native groups (271 cases / 72,979 assertions); it did not run `[ParticlesReadback]`. Public `36ba502e5c` clears rejected sort size output and adds boundary assertions; CI now executes `[ParticlesReadback]` with a nonzero case-count guard. Exact-SHA hosted `41176081184` passed static checks, 2+21 Python tests and eight native groups (273 cases / 73,068 assertions), including `[ParticlesReadback]` 2 cases / 89 assertions. Exact-source standalone red/green arithmetic evidence, successful test/renderer SCU compiles, and full CI log are in `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full local Editor and GPU qualification, original owner/session/typed-close, Ready and G1 obligations remain open; continue other ordinary Godot FR work.

## Particle instance size rejection — 2026-10-31

Public `2a287327f2` clears rejected instance-buffer size output for ordinary and motion-vector doubled allocations above the RD limit. Exact-source standalone red/green arithmetic: old 2 failures / 5 rows, new 0 / 5; test and renderer SCUs compile with `-Werror`, format/diff checks pass. Hosted `41176728214` passed static checks, 2+21 Python tests and eight native groups (273 cases / 73,072 assertions), including `[ParticlesReadback]` 2 cases / 93 assertions. Receipt and full log: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Full local Editor/GPU and original owner, typed-close, Ready and G1 obligations remain open; long queue stays active.

## Particle size-helper rejection outputs — 2026-10-31

Public `d6cf418d7e` clears rejected outputs for particle data, emission, trail bind-pose and frame-parameter RD-limit checks. Four focused `[ParticlesReadback]` assertions added; test and renderer SCUs compile with `-Werror`, format/diff checks pass. Hosted `41177173196` passed static checks, 2+21 Python tests and eight native groups (273 cases / 73,080 assertions), including `[ParticlesReadback]` 2 cases / 101 assertions. Receipt and full log: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. This remains scoped CPU/native boundary work; GPU, owner/session, typed-close, Ready and G1 remain open.

## Particle trail CPU regression execution — 2026-10-31

Public `6edbf89d40` adds existing `[ParticlesTrail]` allocation-failure regression to Linux sanitizer smoke with a nonzero case-count guard. Hosted `41177663139` passed static checks, 2+21 Python tests and nine native groups (274 cases / 73,085 assertions), including `[ParticlesTrail]` 1 case / 5 assertions. The real Vulkan opt-in trail case was not enabled. Receipt and full log: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Original GPU, owner/session, typed-close, Ready and G1 obligations remain open.

## Next particle resource-failure control — 2026-10-31

Current `particles_storage.cpp` source audit: `_particles_update_buffers()` is `void` and can return after a rejected size or failed allocation, while `update_particles()` continues; `_particles_process()` can return on failed uniform creation before setting `clear = false`, and the fixed-FPS loop uses `while (todo >= frame_time || particles->clear)`. A controlled real-RD allocation/uniform failure fixture is needed to reproduce and pin the retry/exit semantics before changing this control flow. This is a source-derived pending case, not a runtime failure claim or completed repair. Other independent Godot FR work continues while the fixture is prepared.

## Particle draw amount overflow — 2026-10-31

Public `77a61c8c82` applies checked particle/trail total arithmetic to the actual `particles_get_amount()` draw count consumed by Mobile, Forward+ and Canvas. Invalid totals return zero with a safe divisor. Focused test-first red compile and final test/renderer SCU compiles pass, as do format/diff checks; exact remote SHA verified. Hosted `41178455665` passed static checks, 2+21 Python tests and nine native groups (274 cases / 73,094 assertions), including `[ParticlesReadback]` 2 cases / 110 assertions. Receipt and full log: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. This does not sign real GPU drawing, owner/session, typed-close, Ready or G1.

## Rebuildable binary capacity recovery — 2026-10-31

Under the prior user authorization to clean confirmed regenerable old build artifacts, 20 historical E22 Editor copies (3,753,488,048 logical bytes) were removed after SHA recording and no-open-file check. The `HANDOFF_A.json` fixed single/double pair remains byte-identical; deletion inventory is `work/e22-native-20261030/OLD_EDITOR_CLEANUP_20261031.json`. Thirty-two K02 release case executable copies were replaced by hard links to the retained exact precision-matched build binaries, preserving each path and SHA; inventory is `work/k02-current-m12-neighbors-20261031/RELEASE_CASE_BINARY_DEDUP_20261031.json`. The saved matrix verifier still passes 64 cases / 304 PNGs / 16,704 sampled pixels / 32 release PCKs. Disk available rose from 2.4 GiB to about 7.9 GiB. All original product and GPU gates remain open.

## Particle Canvas trail field width — 2026-10-31

Public `3f7c529ab9` extends the Canvas particle trail field from 7 to 8 bits, covering the public 128 sections/129 bind-pose boundary and moving adjacent flag bits consistently in C++ and GLSL. Test-first native control failed 1/113 on the old mask, then passed 3/113 after the fix; complete incremental macOS Editor link, `[ParticlesTrail]` 1/5 and opt-in real Vulkan trail retry 1/17 passed. The preceding exact `77a61c8c82` full Editor build passed and its focused native groups passed 2/110, 1/5 and real Vulkan 1/17. A preliminary Metal Canvas trail image probe did not show visible particles even at 8 sections, so actual trail drawing remains unqualified. Evidence and logs: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Resource-failure control flow, complete GPU rendering, K03 typed-close, owner/session, Ready and G1 remain in the active long queue.

Canvas diagnostic follow-up: a corrected offscreen 2D camera/marker probe observed 48 red pixels at 128 sections on local Metal Mobile and Forward+ (and 48 at 8 sections on Mobile); this is a limited visible-particle observation, not full trail-stride or product GPU acceptance. Diagnostic source, logs and image are in the particle receipt.

## 3D particle trail flag width — 2026-10-31

Public `98e42cdf6f` makes shared draw amount reject a trail divisor above 255, matching Mobile/Forward+ 8-bit shader flags; rejected draw amount is zero and divisor one. A 255/256 red-green native control failed 5 assertions before the fix and passed 3 cases/119 assertions afterward. Complete incremental macOS Editor link, trail CPU 1/5 and opt-in real Vulkan retry 1/17 passed. Receipt and logs: `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Unsupported update-path handling and full 3D pixel qualification remain open; the Godot long queue stays active.

E05 source audit: Godot has the original `device_session_bind_main_scene_ready_v1` main RD/session/registry issuer, but the ANGE `read_storage_owner_inputs()` consumer still has no authenticated U08 no-world retained budget/transport source and returns `UNAVAILABLE host.storage_owner_transport`. The typed budget snapshot is only a validation shape; planned owner-source accessors have no accepted runtime issuer. No getter was fabricated from profile constants. This blocks that product gate only; ordinary Godot FR work continues.

Exact Canvas-width commit `3f7c529ab9` hosted run `41180297090` is terminal success: static checks, Linux sanitizer smoke, nine native groups 275/73,097, including particles 3/113 and trail 1/5. Exact successor `98e42cdf6f` CI `41180820650` remains in progress at the recorded check; local verification stands separately.

Exact `98e42cdf6f` hosted run `41180820650` is terminal success: static checks, Linux sanitizer smoke and nine native groups 275/73,103, including particle readback 3/119 and trail 1/5. Full direct CI log is in the particle receipt directory. GPU trail-pixel and original owner/typed-close gates remain separate.

## Particle resource rejection and recovery — 2026-10-31

Public `bc73fadca0` prevents rejected particle buffers from entering the fixed-FPS simulation loop, skips Canvas/Mobile/Forward+ particle draws without a transforms uniform, and permits retry when only a partial buffer set exists. A full incremental macOS Editor build passed; actual Apple Metal Mobile and Forward+ fixtures each passed both 2D and 3D oversized-rejection → amount-1 visible-recovery paths. Native readback 3/119, trail 1/5, and real Vulkan retry 1/17 passed. Exact logs and limits are in `work/particle-gpu-buffer-size-20261031/RECEIPT.md`. Valid-sized device allocation failure was not injected; exact-commit hosted CI is pending identification. Other Godot FR and K03 typed-close work remains in the active long queue.

Exact `bc73` hosted CI `41182450419` failed static checks on the new runner's executable bit and Ruff format, before Linux smoke. Fixture-only follow-up `611d08c4a9` fixes both, passes local Ruff 0.15.8 checks and a rerun of Mobile 2D, and is remote-verified. Successor hosted CI remains pending; the first failure and its log are retained in the particle receipt.

Exact `611d08c4a9` hosted run `41182702993` then completed success: static checks, Linux Editor build, and nine focused native groups passed 275 cases/73,103 assertions. Full direct log is in the particle receipt directory. Metal pixel and opt-in Vulkan results remain local, and full particle/FR/K03 owner qualification stays open.

Public `42294976c9` fixes the normal-sized motion-vector instance-buffer rejection retry path. A real Metal Forward+ same-node fixture was RED with 0 recovered pixels and invalid cached uniform binds after one test-only allocation rejection; it is GREEN with 64 recovered pixels and only the expected rejection. No-injection TAA control and four existing Metal oversized-size fixtures pass. Complete local Editor build, no-test storage SCU compile, particle CPU groups 3/119 and 1/5, and opt-in real Vulkan retry 1/17 pass. Exact source, logs and limits are in `work/particle-gpu-buffer-size-20261031/RECEIPT.md`; hosted CI for `42294976c9` is pending. The long Godot queue remains active.

Exact `42294976c9` hosted run `41184695370` then completed success: static checks, Linux Editor build, and nine focused native groups passed 275 cases/73,103 assertions with zero failures. Full direct log is saved in the particle receipt directory. This CI does not replace local Metal or opt-in Vulkan evidence, and does not close the remaining FR/K03/owner gates.

## Continuation — K03 original production join source audit, 2026-10-31

Exact Godot source `42294976c9` and inspected ANGE source at `e00103fe2b` were traced from SceneReady main-RD issuance through qualification bridge, prepared upload/record, cancel, observation, exact qualification copy, physical retirement and bridge close. The previously reported missing qualification cancel is stale: `device_session_qualification_cancel_v1` is present in current Godot JSON/Registry and ANGE adapter. The first **production** handoff still missing is an original StageOccurrence/operation correlation bound to an actual checked main-RD append/source receipt; qualification ticket identity and `qualification_record_prepared_rt` cannot serve it. `R01_R02_PRODUCTION_JOIN.md` explicitly marks these ports planned. The separate U08 no-world storage-owner source still returns `UNAVAILABLE host.storage_owner_transport` before world entry. Read-only audit and exact source hashes: `work/k03-production-join-audit-20261031/RECEIPT.md`. No product call, typed-close, Ready or G1 completion follows; continue Godot original R02/K01 producer work and independent native FR lanes.

## Continuation — ordinary particle process failure retry, 2026-10-31

Godot commit `7315291e90` propagates `_particles_process()` failure to the pre-process and fixed-FPS callers, stopping the current update and retaining retry state. A test-only one-shot material-uniform allocation refusal ran through real Metal Mobile and Forward+ with one expected rejection and visible recovery; prior size-recovery and motion-retry neighbors passed sequentially. Incremental Editor build and focused particle CPU groups pass. Concurrent Metal windows initially timed out and are excluded from positive results. Exact commit, binary hash, commands and scope are in `work/particle-process-failure-20261031/RECEIPT.md`. Fast-forward push and remote SHA were verified; hosted CI remains pending, and K03 original production and whole GPU acceptance remain open.

Hosted exact-head run `41187113335` was observed `in_progress`; it is not counted as a pass. The long goal remains active across K03 production join, owner import and other original Godot FR work.

## K03 StageOccurrence production join / particle late failure — 2026-10-31 follow-up

Hosted CI `41187113335` for exact Godot SHA `7315291e9061cc884390c57f48cecf83058a6d84` completed `success`; this validates that prior commit only. A later fixed-FPS post-clear failure was reproduced on real Metal Mobile in `/tmp/particle-late-red-mobile-20261031.log`: instance copy was reached after a failed simulation. The isolated candidate in `/tmp/godot-particle-push-20261031` now tracks failure directly and restores retry state, with test-only late rejection and copy gate, but clean GPU GREEN is pending a bounded rebuild. See `work/particle-late-failure-20261031/RECEIPT.md`. This remains a narrow follow-up, not a new ordinary-fix expansion.

K03 remains the next complete native package. Exact current ANGE source lacks C++ `StageOccurrence`/`OperationCorrelation` production issuer and `RecordSession` production arm; Godot has only qualification-scoped append/ticket as inspected. See `work/k03-production-join-audit-20261031/NEXT_SOURCE_FACTS.md`. Implement original operation issuer/adoption and typed main-RD checked append/source→native completion/readback/release; retain fail-closed status until actual same-operation proof. No qualification result is promoted to production/Ready/G1.

The late-failure follow-up above now has local committed SHA `659830f82c37363ec10f171934a7e373f134b825` and exact Metal Mobile/Forward+ GREEN with explicit same-node restart; neighbor fixture results are in its receipt. K03 remains the next full implementation package. No hosted CI for this new SHA is claimed.

Godot `659830f82c37363ec10f171934a7e373f134b825` was fast-forward pushed and exact remote SHA verified. Hosted CI remains pending. The ordinary follow-up is closed at local Metal level; proceed with K03 original production join.

## Particle buffer allocation cleanup follow-up — 2026-10-31

The particle lane has a local uncommitted candidate in `/tmp/godot-particle-push-20261031`: `_particles_update_buffers()` is now a fail-closed boolean contract, and the TESTS_ENABLED motion-vector rejection allocates then releases the temporary instance RID before returning failure while retaining a valid particle buffer. Static checks and the renderer storage SCU compile pass. Runtime re-execution is pending a bounded cached engine link; the interrupted broad incremental build is not counted as pass/fail. Existing `659830f82c` GPU/hosted evidence remains scoped to its committed source. This row is independent of K03 issuer/owner and does not alter typed-close or G1 status.

Particle accessor fail-closed follow-up: `a0e17d2d51` propagates `_particles_update_buffers()` failure through `particles_get_instance_buffer_uniform_set()`, closing the remaining stale-instance-uniform caller. Header format/diff and targeted renderer-storage SCU checks pass; remote SHA is verified. Full runtime remains pending bounded engine linkage; no GPU or product-wide claim is made.

Hosted verification for particle accessor successor `a0e17d2d51`: run `41192327852` completed success with static checks and Fork Linux sanitizer smoke. Other platform jobs were skipped; this does not replace local Metal runtime evidence or close full particle/K03 product gates.

Particle resource-lifetime follow-up `3ab569aeae`: frame-parameter and trail bind-pose buffer replacement now invalidates dependent uniforms, and bind-pose allocation failure stops the update before fallback use. Exact renderer-storage SCU compile, format and diff checks pass; remote SHA verified. Hosted CI and Metal runtime remain separate pending evidence.

Hosted run `41193475278` for particle resource-lifetime commit `3ab569aeae` completed success with static checks and Fork Linux sanitizer smoke. Other platform jobs were skipped; local Metal and full particle/K03 product gates remain separate.

Particle batch continuation `09599705fb`: per-node resource/preparation and dispatch failures now skip only the failed particle and continue the update list, preserving fail-closed behavior without aborting unrelated nodes. Exact renderer-storage SCU compile, format and diff checks pass; remote SHA verified. Hosted and Metal runtime evidence are pending separately.

Hosted run `41194171758` for particle batch-continuation `09599705fb` completed success with static checks, Linux editor build and focused native regressions. Other platforms were skipped; no Metal or full particle/K03 product qualification is inferred.

Particle frame-buffer lifetime fix `8952fbc7da`: clearing the RID after release prevents stale-handle reuse when rebuild sizing/allocation fails. Exact renderer-storage SCU compile, format and diff checks pass; remote SHA verified. Hosted and Metal runtime evidence remain pending separately.

## Particle instance RID rebuild lifetime — 2026-10-31

Public `d739fde4b6` releases a pre-existing instance buffer before rebuild when the copy uniform is invalid, and checks actual uniform validity before treating the buffer set as ready. Test-first RED/GREEN contract, exact renderer-storage compile, format and diff checks pass; exact hosted run `41195931160` is still in progress with static checks complete and Linux sanitizer smoke building. This is scoped to instance-buffer resource lifetime and does not claim Metal runtime, full particle qualification, K03 production typed-close, Ready or G1.

## Particle trail uniform stale-RID guard — 2026-10-31

Public `9412fe4e04` makes trail uniform creation and view-axis processing reject non-null but invalid uniform RIDs. Test-first RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41196259127` is pending. Scope remains particle resource validity; K03 production typed-close, Ready, G1 and full GPU acceptance remain open.

## Particle trail uniform retry reset — 2026-10-31

Public `6c2dcd911a` clears a stale trail uniform RID before retrying uniform creation. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41196397314` is in progress; superseded runs were cancelled and are not counted. Scope remains particle resource lifetime; K03 production typed-close, Ready and G1 remain open.

## Particle sort uniform stale-RID rebuild — 2026-10-31

Public `3ce796010c` rebuilds and clears stale sort buffer/uniform resources when the sort uniform RID is invalid. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41196576167` is in progress; superseded runs were cancelled and are not counted. Scope remains particle sort resource lifetime; K03 production typed-close, Ready and G1 remain open.

## Particle view-sort copy uniform guard — 2026-10-31

Public `82c17c12f7` makes view-axis sort/copy processing reject invalid copy uniform RIDs before binding. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41196854367` is in progress; predecessor run was superseded and cancelled. Scope remains particle fail-closed resource validity; K03 production typed-close, Ready and G1 remain open.

## Particle collision uniform retry reset — 2026-10-31

Public `046d7bf173` clears collision texture uniform state before replacement, preserving fail-closed retry semantics on create failure. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41196984987` is pending; older run may be superseded. Scope remains particle collision resource lifetime; K03 production typed-close, Ready and G1 remain open.

## Particle material uniform retry reset — 2026-10-31

Public `5b582411bf` clears material uniform state before replacement, preserving fail-closed retry semantics. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41197090732` is pending; an older run may be superseded. Scope remains particle material resource lifetime; K03 production typed-close, Ready and G1 remain open.

Hosted exact-SHA run `41197090732` for particle resource-lifetime successor `5b582411bf` completed success: static checks, Linux editor build, build-generator regressions and focused native regressions passed; other platform jobs were skipped by policy. The result covers source/build integration for the accumulated particle stale-RID guards and does not replace Metal/GPU runtime, K03 production typed-close, Ready or G1 evidence.

## Particle transform uniform retry reset — 2026-10-31

Public `4f1223b805` clears transform uniform state before replacement, preserving explicit retry semantics. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41197835119` is in progress; prior `41197090732` succeeded for the predecessor. Scope remains particle resource validity; K03 production typed-close, Ready and G1 remain open.

## Particle update ready guard validity — 2026-10-31

Public `7dd7c22677` makes `update_particles()` stop on an invalid non-null copy uniform before simulation/draw work. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41198012928` is pending; predecessor remains in progress. Scope remains particle fail-closed readiness; K03 production typed-close, Ready and G1 remain open.

Hosted exact-SHA run `41198012928` for particle ready-guard successor `7dd7c22677` completed success: static checks, Linux editor build, build-generator regressions and focused native regressions passed; other platforms were skipped by policy. Scope remains source/build integration only; Metal/GPU, K03 production typed-close, Ready and G1 remain open.

## Particle buffer update Error propagation — 2026-10-31

Public `f9abffb664` propagates `buffer_update()` failures for emission, frame parameters, sub-emitter reset and trail bind poses, preserving fail-closed retry state. RED/GREEN contracts, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41198684298` is in progress; predecessor succeeded. Scope remains particle update failure handling; Metal/GPU, K03 production typed-close, Ready and G1 remain open.

Hosted exact-SHA run `41198684298` for particle buffer-update successor `f9abffb664` completed success: static checks, Linux editor build, build-generator regressions and focused native regressions passed; other platforms were skipped by policy. Scope remains source/build integration only; Metal/GPU, K03 production typed-close, Ready and G1 remain open.

## Particle process pipeline validity guard — 2026-10-31

Public `2f4f7f43a7` rejects invalid particle material pipelines and shared base uniform sets before compute dispatch. RED/GREEN source contract, exact renderer-storage compile, format/diff checks and remote SHA verification pass. Hosted `41199327469` is in progress; predecessor succeeded. Scope remains particle fail-closed dispatch; Metal/GPU, K03 production typed-close, Ready and G1 remain open.

Hosted exact-SHA run `41199327469` for particle process-pipeline successor `2f4f7f43a7` completed success: static checks, Linux editor build, build-generator regressions and focused native regressions passed; other platforms were skipped by policy. Scope remains source/build integration only; Metal/GPU, K03 production typed-close, Ready and G1 remain open.

## Particle copy pipeline validity guards — 2026-10-31

Public `cff520c3c8` rejects invalid sort-copy and instance-copy compute pipelines before binding/dispatch in `particles_set_view_axis()` and `update_particles()`. Test-first RED/GREEN source contract, clang-format, diff check, exact macOS editor build (rc 0), push and remote SHA verification pass. Hosted exact-SHA run `41202079561` is in progress. Scope remains particle fail-closed dispatch; Metal/GPU runtime, K03 production typed-close, Ready and G1 remain open.

Focused native follow-up for `cff520c3c8`: `[ParticlesReadback]*` 3/119 and `[ParticlesTrail]*` 1/5 passed rc 0 on the rebuilt macOS Editor. This is headless native coverage only; Metal/GPU runtime, K03 production typed-close, Ready and G1 remain open.

Hosted exact-SHA run `41202079561` for `cff520c3c8` completed success: static checks and Fork Linux sanitizer smoke passed; remaining platform jobs were skipped by workflow policy. The result covers source/build integration for copy-pipeline validity guards; Metal/GPU runtime, K03 production typed-close, Ready and G1 remain open.

## Particle process retry and rollback continuation — 2026-11-01

The local particle continuation now carries retry state explicitly: a deferred fixed-FPS retry records the rendering frame that committed its delta, so same-frame retries do not double-count wall-clock time while a later frame adds its new delta. Pre-process initialization carries a remaining-time value and restores `clear` after partial failure, avoiding replay of completed initialization steps. `_particles_process()` only restores frame history after the history shift has actually occurred, preventing early resource failures from corrupting untouched trail history. The update queue continues healthy systems while requeueing failed nodes.

Explicit restart now invalidates any stale frame-delta retry marker and starts a fresh simulation epoch. Focused source-extracted fixtures pass fresh: process progress `49/49` in float and double precision, dispatch `9/9`, deferred retry `2/2`, update queue `3/3`, buffer retry `16/16`, heightfield size `4/4`, and setter bounds `31/31`; clang-format and `git diff --check` pass. These are bounded CPU/source-contract receipts; no Metal/Vulkan runtime or full particle/K03 qualification is inferred. The long Godot native FR queue remains active.

## Particle resource epoch reset continuation — 2026-11-01

`_particles_free_data()` now invalidates the fixed-FPS remainder, deferred-retry marker, committed-frame marker, and partial pre-process state whenever a full particle resource epoch is torn down. `update_particles()` rechecks the retry candidate after buffer preparation and explicitly disables it when resource creation requests a restart, so userdata/material and setter-triggered rebuilds cannot suppress the new epoch's frame delta. The static `particle_epoch_reset` fixture passes `8/8` as supplemental source audit; the source-extracted process harness now executes same-frame retry with and without simulated resource teardown plus explicit restart, and passes `51/51` in both float and double precision. The focused continuation also passes process dispatch `9/9`, deferred retry `2/2`, update queue `3/3`, buffer retry `16/16`, heightfield size `4/4`, and setter bounds `31/31`. `git diff --check` and clang-format dry-run pass. Scope remains bounded source/CPU contracts; no Metal/Vulkan runtime, full particle qualification, K03 typed-close, Ready, or G1 claim is made.

## Particle tests=yes runtime continuation — 2026-11-02

The current dirty particle source scope was rebuilt with `scons tests=yes platform=macos target=editor -j1` (rc 0). Source-matched native doctest selections pass: `[GPUParticles]*` 15 cases / 79 assertions, `[CPUParticles]*` 8 / 30, `[Particles]*` 4 / 9, and `[ParticlesProcessRetry]*` 3 / 44; every command returned rc 0. All 15 source-extracted particle fixtures were rerun with rc 0. The same tests-enabled editor passes the Metal 4.0 Forward+ 230-frame lifecycle smoke: active/restarted/resumed each produce 690 red pixels, stopped/freed produce 0, AABB contracts shrink then clear, and queue-free reports `invalid=true`. Exact source hashes, binary SHA, commands, logs and limitations are recorded in `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

This closes the bounded particle scene/storage/retry/resource-sync runtime evidence for the current source window. It does not claim full particle GPU qualification, K03 production operation/owner, typed-close, Ready or G1; those remain separate active queue rows.

The GPUParticles doctest process emits one RendererDummy DummyShader RID leak diagnostic at exit despite 79/79 assertions and rc 0; it is retained as a bounded test-environment limitation and does not appear in the Metal smoke run.

After the dependency-notification changes, all 16 source fixtures were rerun from the current tree and returned rc=0, including the GLES3 setter harness and both draw-pass dependency checks.

## Particle draw-pass dependency invalidation — 2026-11-02

RD `particles_set_draw_pass_mesh()` now emits `DEPENDENCY_CHANGED_PARTICLES` after replacing a draw-pass RID, and draw-pass count changes plus sub-emitter replacement now invalidate the same dependency. The source fixture `particle_draw_pass_dependency` checks both RD and GLES3 paths, both process-material replacements, and RD sub-emitter self-reference fail-closed behavior, passing 8/8; the existing storage setter fixture remains 36/36, the scene particle groups remain green, and the Metal smoke remains green. The updated evidence is included in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` with 16 source fixtures.

## Physics callback epoch and native body retirement continuation — 2026-11-02

The built-in GodotPhysics2D/GodotPhysics3D and Jolt Physics callback paths now defer body destruction while a backend query callback is active. `free_rid()` marks the body pending, preserves the RID owner, body storage, and direct-state lifetime for the active callback, and drains the physical detach/free only after the complete backend query flush. Pending bodies are skipped if their query-list turn has not started yet. Callback setter reentry remains epoch-staged and revision-checked.

A source-matched tests=yes Editor build passed. The focused matrix covers 12 commands: GodotPhysics2D and GodotPhysics3D native retirement in threaded/non-threaded modes, Jolt native retirement in both modes, callback epoch controls, and Godot/Jolt backend dispatch controls; all returned rc=0 with doctest success. Evidence and source hashes are fixed in `work/physics-query-callback-epoch-20261102/RECEIPT.json`.

The aggregate selections also pass GodotPhysics2D 6 cases/138 assertions in each thread mode and GodotPhysics3D 7 cases/155 assertions in each thread mode.

This closes the bounded body callback-retirement behavior for these three backends. Physics wrapper finish, complete shutdown/shape/area/joint retirement, owner integration, typed-close, Ready and G1 remain separate active rows.

## GLES3 unsupported-trail input validation — 2026-11-02

The Compatibility renderer now validates trail lifetime and trail bind-pose transforms before emitting its unsupported-feature warning, matching RD fail-closed input behavior. The GLES3 setter fixture passes 10/10, the complete particle fixture matrix remains green, and the current Metal smoke remains unaffected.

## CPU particle base-color finite guard — 2026-11-02

`CPUParticles2D::set_color()` and `CPUParticles3D::set_color()` now reject NaN and infinite channels before mutating the simulation color. The regression test preserves the prior valid color after invalid writes; the current tests=yes editor reports `[CPUParticles]*` 9 cases / 32 assertions and `[GPUParticles]*` 15 / 79, with `[Particles]*` 4 / 9 and `[ParticlesProcessRetry]*` 3 / 44 also passing. All 16 source fixtures and the 230-frame Metal Forward+ smoke remain green. Updated evidence is in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (SHA-256 `bbcfaa9d2a3bf969b160d53e98568f743f4c88952c7602cf751467d4e2474213`); the physics callback receipt was refreshed against the same current binary (SHA-256 `3131c100a0d434a68fbf004ad1704bc1f62efaf263387a123452442306f05643`).

## Active Reload instance snapshot replay — 2026-11-02

The committed `GDExtension::prepare_reload()` capture fix snapshots tracked `ObjectID`s and reacquires each object after property-list, getter, and default-value callbacks, so callback-driven untracking/destruction cannot invalidate the iteration. The current tests=yes editor replays `device_exit::method/reload-capture-survives-property-callback-destruction` with three routes (sibling deletion, receiver deletion, and default-value deletion): 1 case / 33 assertions, all passed, with observed `deletions=1` and the expected getter/default-getter counters. This remains a bounded capture contract; manager preflight ordering, self-destruction inside Object/ClassDB internals, complete hot reload, typed-close, Ready and G1 remain separate rows.

## Active Reload finish-reload snapshots — 2026-11-02

`GDExtension::finish_reload()` now uses independent instance-ID snapshots for reset, property restoration and `NOTIFICATION_EXTENSION_RELOADED`; restoration copies properties before callbacks and reacquires objects before each setter. The new source checker and existing prepare-reload checker are GREEN, and the direct capture native route remains 1 case / 33 assertions, rc0. Manager preflight/G1, complete Active Reload, typed-close, Ready and G1 remain separate active rows.

The current binary also passes the complete `device_exit::method/*` neighbor selection at 10 cases / 515 assertions, rc0; the three callback-scope error lines are expected negative initialization diagnostics.

## ParticleProcessMaterial setter bounds — 2026-11-02

`ParticleProcessMaterial::set_sub_emitter_frequency()` now rejects zero, negative, NaN and infinite values before computing the shader delta, and `set_emission_point_count()` rejects negative counts before publishing the material parameter. The regression preserves valid state after invalid writes; `[Particles]*` passes 5 cases / 11 assertions, while the other particle groups, all 16 source fixtures, and the 230-frame Metal Forward+ smoke remain green. The particle receipt now includes `scene/resources/particle_process_material.cpp` in its source scope and records the current binary SHA-256 `f3199f3f0e7db99097faf75a739d495e2ff421eeffa13b0996584fdcda617731`.

## ParticleProcessMaterial finite input guards — 2026-11-02

The GPU process-material direction, spread, flatness, velocity pivot, base color, inherited velocity ratio, gravity and lifetime-randomness setters now reject non-finite values before mutating state or publishing shader parameters. The new state-preservation regression passes as part of `[Particles]*` (6 cases / 19 assertions); all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `1fdb683e1c2525d71149cd2a1f666a13a8281b8c1586161ce5212ed5d24b251c`.

## ParticleProcessMaterial emission geometry guards — 2026-11-02

Emission sphere radius, box extents, ring axis/height/radius/inner-radius/cone-angle, and shape offset/scale now reject non-finite values before changing material state or publishing shader parameters. The geometry regression passes with `[Particles]*` at 7 cases / 28 assertions; all other particle groups, 16 source fixtures, Metal smoke and the serialized physics callback matrix remain green. Current binary SHA-256 is `2bbf6890c11bfb44082e920f326af4d790ace1ecfe4bbc76891e7717fab556ff`.

## ParticleProcessMaterial turbulence and collision guards — 2026-11-02

Turbulence strength, scale, speed randomness/speed and collision friction/bounce now reject non-finite values; turbulence scale also rejects negatives before fractional-power conversion. The regression passes with `[Particles]*` at 8 cases / 34 assertions; all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `d18dfa96d45f0e8fb714874f1fc526dc84d68130dcc14f26271d69886d891595`.

## ParticleProcessMaterial parameter bounds — 2026-11-02

The shared `set_param_min/max()` path now rejects non-finite values before updating any particle velocity, acceleration, scale or animation parameter. The state-preservation regression passes with `[Particles]*` at 9 cases / 36 assertions; all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `2002ff48bd69ee26c790289fb651ab33e92ccb0a65a999cb02fd86e3b3c5c5f4`.

## ParticleProcessMaterial 3D vector bounds — 2026-11-02

3D scale, rotation and rotation-velocity min/max setters now reject non-finite vectors before range reconciliation or shader publication. The regression passes with `[Particles]*` at 10 cases / 42 assertions; all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `05a013052d3b1aed854934a655074cb3bd7ee3f3199a9fd7080de403da2d8363`.

## ParticleProcessMaterial sub-emitter bounds — 2026-11-02

`SubEmitterMode` now rejects invalid enum values, and the three sub-emitter count setters reject negative counts before shader publication. The regression passes with `[Particles]*` at 11 cases / 46 assertions; all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `ad27a825b14d82064765bce5339a6231a362c82e418eb94a75176369731ae99e`.

## ParticleProcessMaterial collision-mode bounds — 2026-11-02

`set_collision_mode()` now rejects invalid `CollisionMode` values before shader regeneration and property-list updates. The regression passes with `[Particles]*` at 12 cases / 47 assertions; all other particle groups, 16 source fixtures and the Metal lifecycle smoke remain green. Current binary SHA-256 is `9fbcd1d595ef15e421bfc17ab85d28b50f2494292f5ab07f91e8f1cb02f9663f`.

## Particle collision storage finite guards — 2026-11-02

RD and Compatibility particle collision storage now reject non-finite sphere radii, box extents, attractor parameters and collision-instance transforms before state mutation. The new source fixture passes 12/12 checks; the full particle fixture matrix now has 17 green entries and the Metal smoke remains green. Current binary SHA-256 is `14d91bb452d98c7f496b310697eb75e57f66e224ad6a23c865aea108d4237c66`.

The matching scene-layer collision setters now preserve their previous radius, size, thickness and attractor values when given non-finite input. The native `[Particles]*` selection passes 13 cases / 55 assertions, and the collision source fixture plus 17-entry matrix remain green. Current binary SHA-256 is `fb8eaf399e8f5f405dcf6f66725e7022083e84f74a6ecab25bb22029f8e86160`.

Collision type enum inputs are now range-checked in both renderer storages; the collision setter fixture reports 14/14 checks and the 17-entry matrix remains green. Current binary SHA-256 is `aee4b4e4e03ea84f04a4d7c79cf809129b6c14ed937a843f0ccd0dd6be97b517`.

Scene collision resolution/update-mode enums now fail closed as well. The native `[Particles]*` selection passes 13 cases / 58 assertions, the collision fixture remains 14/14, all 17 fixtures are green, and the Metal smoke remains green. Current binary SHA-256 is `1510d30de3587e376652588de2b7e7eab5b828d8877fb0cf848726e63ddf0f15`.

## Particle collision scene enums and emission input bounds — 2026-11-02

The particle scene collision setters now reject out-of-range SDF resolution, heightfield resolution and update-mode enum values before state mutation; the native `[Particles]*` selection remains green at 13 cases / 58 assertions. RD and Compatibility `particles_emit()` now reject non-finite transform, velocity, color and custom inputs before the unsupported-feature path. The new `particle_emit_input_bounds` source fixture passes 2/2, the collision setter fixture passes 14/14, and the complete source-extracted particle matrix has 18/18 rc-zero entries.

The current tests-enabled macOS Editor (`scons tests=yes platform=macos target=editor -j1`, rc 0) passes `[GPUParticles]*` 15 cases / 79 assertions, `[CPUParticles]*` 9 / 32, `[Particles]*` 13 / 58, and `[ParticlesProcessRetry]*` 3 / 44. The same binary passes the Metal 4.0 Forward+ 230-frame lifecycle smoke: active/restarted/resumed each produce 690 red pixels, stopped/freed produce 0, AABB contracts and clears after free, and the freed node reports `invalid=true`. Evidence is in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` with 18 scoped source files and 18 green source fixtures.

This closes the bounded particle setter, collision-scene enum, manual-emission input, retry/resource-sync and Metal lifecycle evidence for this source window. It does not claim full GPU qualification, typed-close, K03 production operation/owner, Ready or G1; those remain active queue rows.

## GLES3 particle amount capacity guard — 2026-11-02

`drivers/gles3/storage/particles_storage.cpp::particles_set_amount()` now rejects amounts whose maximum 3D process-buffer allocation would exceed the `uint32_t` buffer-size limit before freeing/rebuilding particle resources. The guard is conservative for 2D and protects the process and instance allocation path without changing the public node's normal amount range. The RED→GREEN checker `work/particle-gles3-amount-overflow-20261102/check_gles3_amount_guard.py` passes, the incremental tests-enabled macOS Editor rebuild returns rc 0, and the current binary passes all four particle doctest groups plus the 18-entry source fixture matrix.

The updated Metal 4.0 Forward+ smoke remains green at 230 frames with 690 red pixels in active/restarted/resumed, zero in stopped/freed, shrinking/cleared AABB, and `invalid=true` after queue-free. Current evidence is in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `c63704f2267bdb48a28f5a3b0ff712467068df8cd35368d9b329bd9656706c2c`; 19 scoped source/checker files, 18 source fixtures).

## Particle collision geometry non-negative bounds — 2026-11-02

Collision and attractor scene geometry setters now reject negative sphere radii, box/vector-field dimensions, SDF thickness and collision sizes before mutating state or sending negative extents to RenderingServer. Zero remains accepted for compatibility with degenerate geometry. Native `[Particles]*` now passes 13 cases / 61 assertions, and the new `particle_collision_scene_bounds` source fixture passes 8/8 alongside the other 18 fixtures.

A fresh tests-enabled macOS Editor incremental build returns rc 0, and the same binary passes the 230-frame Metal Forward+ lifecycle smoke with the existing pixel/AABB/free-node checks. Current evidence is in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `752c0c0f917979720c18c29bc78577ffd850ba0361b0663ca1c49d9c27cd79ae`; 20 scoped source/checker files, 19 source fixtures).

## Particle collision storage non-negative bounds — 2026-11-02

The RD and Compatibility particle collision storage setters now reject negative sphere radii and negative box extents before mutating collision state or notifying AABB dependencies. This keeps direct RenderingServer calls aligned with the scene-layer collision geometry contract. The storage fixture remains 14/14, the scene geometry fixture remains 8/8, all 19 source fixtures pass, and `[Particles]*` remains 13 cases / 61 assertions.

A fresh incremental tests-enabled Editor build returns rc 0 and the current Metal Forward+ lifecycle smoke remains green at 230 frames. Evidence is synchronized in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `829e1782ec71f0b5ff39be89ba61e97904c002461c632283e51821c869da2fd0`).

## RD particle lifetime-split arithmetic guard — 2026-11-02

The RD particle copy paths no longer narrow `amount * phase` directly to `int`. A checked `_particles_get_lifetime_split()` validates amount, phase sign/finite state, scaled-phase finiteness and endpoint saturation before writing the copy push constant. View-dependent copy returns before dispatch on rejection; regular update queues the particle for retry. `[ParticlesReadback]*` now passes 11 cases / 58 assertions, including zero, endpoint, negative, NaN and overflow cases.

The current tests-enabled Editor also passes the four scene/process particle groups, all 19 source fixtures, and the 230-frame Metal Forward+ smoke. Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `78b28fe55895f09a812f6010e1f3a0da7d1813efd2d866965bb71fa4a337ac10`).

## GLES3 lifetime-split parity — 2026-11-02

Compatibility renderer lifetime-order copy and reverse-sort paths now use a checked `_particles_get_lifetime_split()` helper before transform-feedback or mapped-buffer work. Invalid phase arithmetic fails closed without issuing the copy/sort operation. The new `particle_gles3_lifetime_split` source fixture passes 3/3; all 20 source fixtures and the five particle doctest groups remain green.

The current tests-enabled Editor and Metal Forward+ smoke remain green. Evidence is synchronized in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `b1cadb6d3cf137bf6892d669789063c8e8c7c9d3cf3be48452ab28bae3b7eb30`).

## ParticleProcessMaterial emission geometry non-negative/range bounds — 2026-11-02

`ParticleProcessMaterial` emission sphere radius, box extents, ring height/radii and cone angle now reject negative or out-of-range values before shader publication. Cone angle is constrained to 0–90 degrees, matching the property contract and shader geometry. The existing `[Particles]*` state-preservation test now exercises these cases; all five particle doctest groups and 20 source fixtures remain green.

The current tests-enabled Editor and Metal Forward+ smoke pass unchanged. Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `545084dc44bf2380992071c82095ffc7f94ac8365fad5f7c64b3933b9cd40bd7`).

## ParticleProcessMaterial parameter range bounds — 2026-11-02

Particle process-material setters now enforce declared ranges for spread (0–180°), flatness (0–1), lifetime randomness (0–1), collision friction (0–1) and collision bounce (0–1), preserving prior state on rejected finite values. The existing `[Particles]*` regression exercises both out-of-range directions; all five particle doctest groups, 20 source fixtures and Metal smoke remain green.

Current evidence is synchronized in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `b3c22f283ce2ae4342a0b1e87105c0ed6baf4d7c204446baf6211899b42ed6af`).

## CPU lifetime-randomness conversion parity — 2026-11-02

CPU 2D/3D `lifetime_randomness` setters now enforce the same 0–1 range as `ParticleProcessMaterial`, preventing CPU→GPU conversion from silently losing an out-of-range value when the material setter rejects it. The new `[CPUParticles]*` regression passes 10 cases / 34 assertions; the remaining particle groups, 20 source fixtures and Metal smoke remain green.

Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `93310f7506833a9c0e998568b192cdc886558d0932ffbebcb46658310d5f8ddf`).

## RD instance-uniform rebuild fail-closed guard — 2026-11-02

`particles_get_instance_buffer_uniform_set()` now checks the instance buffer after `_particles_update_buffers()` and returns a null RID when allocation/rebuild fails, preventing uniform creation against a missing buffer. The new source fixture passes 3/3; all 21 source fixtures, five doctest groups and Metal smoke remain green.

Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `b347fbffeeedb42095f60913ad38994e6b37a7f57b5f68311d3e0f667267e07a`).

## RD AABB readback null-buffer guard — 2026-11-02

`particles_get_current_aabb()` now returns an empty AABB before calling `buffer_get_data()` when the particle GPU buffer has not been allocated. The RED→GREEN `particle_aabb_readback_guard` fixture passes 2/2; all 22 source fixtures, five particle doctest groups and Metal smoke remain green.

Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `8af60bd9029802d7763cf77cc73d74c1c88b7547fdbaeab2fef60e26fc4ee80a`).

## GLES3 AABB/current-data readback guards — 2026-11-02

Compatibility particle `particles_get_current_aabb()` and `particles_get_current_data()` now return empty values before reading an unallocated sort/back instance buffer. The new source fixture passes 4/4; all 23 source fixtures, five doctest groups and Metal smoke remain green.

Receipt: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `3daf1eebe5f54649b1a0e8917f77dc44eda0536949a1c20154f59616a619ad09`).

## GLES3 2D particle AABB layout parity — 2026-11-02

Compatibility particle AABB readback now selects the 2D or 3D instance stride before decoding GPU data. The 2D path uses `ParticleInstanceData2D` and the packed origin at xform indices 3/7, avoiding 3D-sized reads that skipped or overran 2D entries. The new `particle_gles3_aabb_mode_layout` source fixture passes 4/4; the selected-buffer null guard remains covered by `particle_gles3_aabb_readback_guard` at 4/4; all 24 scoped source fixtures, five particle doctest groups and the 230-frame Metal Forward+ smoke remain green.

Evidence is synchronized in `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `c4b57d7ca3558c53cca11b53888c4d77e4b44973eda7b3bb63d0ba1f2f9bcebe`; 26 scoped source files, 24 source fixtures).

## RD collision uniform retry rollback — 2026-11-02

RD particle processing now tracks whether the collision descriptor set was rebuilt during the frame. If that frame fails, the newly rebuilt set is freed and its RID cleared before restoring the CPU collision cache, forcing the next retry to rebuild the descriptor for the restored tuple. The RED→GREEN `particle_collision_uniform_retry` fixture passes 7/7; all 26 source fixtures, five particle doctest groups and the Metal smoke remain green.

Evidence: `work/particle-collision-uniform-retry-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `0a105b5bc78dd0d9cfd39dcb0514e7862963690b8e0bba706da4b16521fbd5f0`).

## GLES3 heightfield framebuffer sizing fallback — 2026-11-02

Compatibility heightfield collision framebuffer sizing now handles zero, negative and non-finite extents with a positive square resolution. Aspect-ratio sizing clamps the thin axis to at least one texel, preventing NaN-to-integer conversion and zero-sized texture allocations. The RED→GREEN `particle_gles3_heightfield_size` fixture passes 5/5; all 26 source fixtures, five particle doctest groups and the Metal smoke remain green.

Evidence: `work/particle-gles3-heightfield-size-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `0a105b5bc78dd0d9cfd39dcb0514e7862963690b8e0bba706da4b16521fbd5f0`).

## Particle ratio range parity — 2026-11-02

CPU/GPU particle nodes and RD/GLES3 storage now enforce the declared `[0, 1]` contract for amount ratio, explosiveness ratio and randomness ratio, preserving the previous value on rejection. Existing ratio regressions and setter fixtures now cover negative, above-one and non-finite inputs. RED→GREEN evidence is recorded in `work/particle-ratio-bounds-20261102/RECEIPT.md`; the final tests-enabled binary passes all five particle selections and Metal smoke.

Evidence: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `7719c8b14168cff286b95355f790482cc29dd8688be999adcc07195738cd368c`, 29 scoped source files, 26 source fixtures).

## ParticleProcessMaterial emission/turbulence upper bounds — 2026-11-02

`ParticleProcessMaterial` now enforces the declared upper bounds for emission point count (0–1,000,000), turbulence noise strength (0–20) and turbulence noise speed randomness (0–4), preserving prior state on rejection. The RED→GREEN source checker passes 3/3 and the native `[Particles]*` selection remains 13 cases / 61 assertions.

Evidence: `work/particle-material-range-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `9deb4c6aea1b153036cb43f63c5cd36eda3e09cb0a173c71e964d397cab0cfb5`, 30 scoped source files, 27 source fixtures).

## Particle collision base-size non-negative contract — 2026-11-02

GPUParticles2D/3D and RD/GLES3 particle storage now reject negative collision base sizes before publishing them to the collision shader path. The new source harness passes 6/6 behavior checks and 4/4 source checks; `[ParticlesProcessRetry]*` covers the RD validator. Final Metal smoke remains green.

Evidence: `work/particle-collision-base-size-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `a032e8733b67463d7bd940eb6894c5c939815b608f50d6409c5b25ae45b525cd`, 31 scoped source files, 28 source fixtures).

## Particle attractor directionality range parity — 2026-11-02

`GPUParticlesAttractor3D` and the RD/GLES3 particle collision storage now enforce the declared finite `[0, 1]` directionality range and preserve the prior value on invalid writes. Native `[Particles]*` passes 13 cases / 61 assertions, and `particle_collision_setter_bounds/run.py` passes 14/14. The current tests-enabled macOS Editor and 230-frame Metal Forward+ lifecycle smoke remain green with binary SHA-256 `19c5d73cab05ca16da33355e6421b1f84b2fcd4dfddc6def5752f620f8380a28`. Evidence: `work/particle-collision-directionality-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## CPU particle geometry range parity — 2026-11-02

`CPUParticles2D/3D` now reject non-finite, negative and above-contract geometry values before publishing CPU particle state: spread is `[0,180]`, flatness is `[0,1]`, emission radii/extents/heights are non-negative, and ring cone angle is `[0,90]`. The RED→GREEN `cpu_particle_range_bounds` source fixture passes 20/20; rebuilt `[CPUParticles]*` passes 10 cases / 43 assertions. Evidence: `work/particle-cpu-range-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## GPUParticles3D process-material gizmo refresh — 2026-11-02

`GPUParticles3D::set_process_material()` now refreshes editor gizmos after publishing the replacement material and updating configuration warnings, so the new emission shape appears immediately. The source fixture is RED at `HEAD` (2/4) and GREEN currently (4/4). The tests-enabled Editor and Metal Forward+ smoke remain green. Evidence: `work/particle-process-material-gizmo-sync-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## ParticleProcessMaterial sub-emitter frequency/count upper bounds — 2026-11-02

`ParticleProcessMaterial` now enforces the declared sub-emitter frequency range `[0.01,100]` and count range `[1,32]` for end, collision and start emissions, preserving the previous values before shader publication. The updated source fixture passes 7/7 and the rebuilt `[Particles]*` selection passes 13 cases / 61 assertions. Evidence: `work/particle-material-range-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## Particle attractor attenuation range parity — 2026-11-02

`GPUParticlesAttractor3D` and RD/GLES3 particle collision storage now enforce the declared finite `[0,8]` attenuation range, preserving prior state on invalid writes before shader publication. `particle_collision_setter_bounds/run.py` passes 14/14; the rebuilt `[Particles]*` selection and 230-frame Metal Forward+ lifecycle smoke remain green. Evidence: `work/particle-attractor-attenuation-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## Particle SDF collision thickness range parity — 2026-11-02

`GPUParticlesCollisionSDF3D::set_thickness()` now rejects non-finite, negative and above-two values to match its declared `[0,2]` meter property range. The collision scene fixture passes 9/9, all five particle doctest groups pass, and the Metal lifecycle smoke remains green. Evidence: `work/particle-sdf-thickness-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## CPUParticles3D mesh resource sync — 2026-11-02

`CPUParticles3D::set_mesh()` now tracks the mesh resource's `changed` signal, disconnecting the old mesh and refreshing configuration warnings for the replacement. The source fixture is RED at `HEAD` and GREEN at 2/2 on the current source; rebuilt `[CPUParticles]*` passes 11 cases / 47 assertions, and the Metal lifecycle smoke remains green. Evidence: `work/particle-cpu-mesh-sync-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## Particle preprocess non-negative range parity — 2026-11-02

CPU/GPU particle nodes and RD/GLES3 storage now reject negative preprocess and requested process times, matching the declared lower bound of zero while retaining finite/representable checks. The RED→GREEN `particle_preprocess_bounds` fixture passes 6/6; rebuilt `[CPUParticles]*` passes 12/49 and `[ParticlesReadback]*` passes 11/58. Metal lifecycle smoke remains green. Evidence: `work/particle-preprocess-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## Particle speed-scale non-negative range parity — 2026-11-02

CPU/GPU particle nodes and RD/GLES3 storage now reject negative speed scales, aligning public state with the renderer's zero-clamped processing and the declared property lower bound. The RED→GREEN `particle_speed_scale_bounds` fixture passes 6/6; rebuilt `[Particles]*` passes 14/65 and `[ParticlesReadback]*` passes 11/58. Metal lifecycle smoke remains green. Evidence: `work/particle-speed-scale-bounds-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## GPU particle sub-emitter post-enter-tree rebinding — 2026-11-02

`GPUParticles2D/3D` now retry sub-emitter attachment during `NOTIFICATION_POST_ENTER_TREE`, covering sibling insertion order where `NOTIFICATION_ENTER_TREE` runs before the target node is available. The RED→GREEN `particle_sub_emitter_post_enter` fixture passes 2/2; all six particle doctest groups and Metal lifecycle smoke remain green. Evidence: `work/particle-sub-emitter-post-enter-20261102/RECEIPT.md` and `work/particle-runtime-smoke-final-20261102/RECEIPT.json`.

## Physics callback epoch current-binary replay — 2026-11-02

The current tests-enabled macOS Editor (binary SHA-256 `150bf4e6358d5dac4fb90214bc7690e9bec0e9ea9756d946cd54163a941db6b9`) reran the existing GodotPhysics2D, GodotPhysics3D and Jolt callback-epoch/native-retirement selections under both non-threaded and threaded environment configurations. All 12 serialized commands returned rc=0: GodotPhysics2D callback/retirement 6 cases / 138 and 2 / 50 assertions per thread mode, GodotPhysics3D callback/retirement 4 / 88 and 2 / 50 per thread mode, and Jolt dispatch/retirement 1 / 17 and 2 / 50 per thread mode. Evidence is in `work/physics-query-callback-epoch-20261102/RECEIPT.json` and `current-binary-rerun.json`; complete physics shutdown, wrapper finish, shape/area/joint paths, owner/session, typed-close and G1 remain separate queue rows.

## GLES3 particle update retry boundary — 2026-11-02

Compatibility particle updates now defer nodes whose process or instance buffers are incomplete after `_particles_update_buffers()`. A per-call retry list lets the current batch continue and hands failed nodes back to the main update queue only after the pass, avoiding invalid transform-feedback use and same-frame retry loops. The RED→GREEN source fixture `particle_gles3_update_queue` passes; the tests-enabled Editor build and six particle doctest selections remain rc=0, all 36 source fixtures pass, and the Metal Forward+ lifecycle smoke remains green. Evidence: `work/particle-runtime-smoke-final-20261102/RECEIPT.json` (binary SHA-256 `6777185662bfd9cd7e0976b1151cd06e4b43f1479ebdc0decd2b6beefe48eb67`). No live GLES3 allocation-failure or full GPU qualification claim is made.

## Physics receipt refresh after GLES3 retry build — 2026-11-02

After the GLES3 particle retry rebuild, the current binary SHA-256 `6777185662bfd9cd7e0976b1151cd06e4b43f1479ebdc0decd2b6beefe48eb67` reran the same 12 serialized GodotPhysics2D/GodotPhysics3D/Jolt callback-epoch and native-retirement commands; all returned rc=0. The physics receipt and queue manifest now bind to this binary. Scope remains the bounded callback-retirement lane only.

## GLES3 retry final binary replay — 2026-11-02

The final serial rebuild after the VAO completeness guard produced binary SHA-256 `19dd787c64cc5f4663d0c59d49ba16ee8e9bb1c8d1a4070b865b558adfd015aa`. All six particle doctest selections returned rc=0; the GLES3 retry fixture is GREEN with its HEAD baseline RED; all 36 source fixtures pass; and the 230-frame Metal Forward+ lifecycle smoke remains green. Physics callback-epoch/native-retirement replay (12 serialized backend/thread commands) and Active Reload direct/neighbor selections also pass on this binary. Evidence is synchronized in the particle and physics receipts; full GLES3 GPU allocation-failure qualification remains open.

## Requested process-time non-negative parity — 2026-11-02

`CPUParticles2D/3D::request_particles_process()` and `GPUParticles2D/3D::request_particles_process()` now reject negative process and residual times before mutating CPU state or forwarding to RenderingServer. The existing preprocess fixture expanded from 6 to 10 checks: HEAD remains RED and current source is GREEN; all 36 source fixtures, six particle doctest groups, and the Metal Forward+ lifecycle smoke pass on binary SHA-256 `0c78578cd80c5a25664f20381394ba093582e791be72ba0fe4507c3574bb5bec`.

## Native process-request regression coverage — 2026-11-02

The native `[GPUParticles]*` regression now includes negative primary and residual `request_particles_process()` inputs for both 2D and 3D public nodes. The final tests-enabled Editor build passes `[GPUParticles]*` 15/79, `[CPUParticles]*` 12/49, `[Particles]*` 14/65, `[ParticlesProcessRetry]*` 3/45, `[ParticlesReadback]*` 11/58 and `[ParticlesTrail]*` 1/5; the 230-frame Metal smoke and all 36 source fixtures remain green on binary SHA-256 `95862f48fade7320afd14e50b46e13cbedca3d4a8e51d2ba3c6d9b290b70fdf7`.

## Sub-emitter invalid-target fail-closed lifecycle — 2026-11-02

`GPUParticles2D/3D::_attach_sub_emitter()` now resolves a candidate RID first and always publishes either that valid sibling particle RID or an empty RID. Missing, self-referential and wrong-type targets therefore clear stale backend links during enter/post-enter rebinding. The expanded fixture is HEAD RED `0/4`, current GREEN `4/4`; all 36 source fixtures, six particle doctest groups and Metal Forward+ smoke remain green on binary SHA-256 `c5d60886e5c108e71e2566d693c3de1da28424f37e1077650347a6e233c9cf8d`.

## Resource changed-connection runtime receipt refresh — 2026-11-02

The existing native resource-sync tests now have current tests-enabled runtime evidence: the `[GPUParticles]*` selection passes 15 cases / 79 assertions on binary SHA-256 `c5d60886e5c108e71e2566d693c3de1da28424f37e1077650347a6e233c9cf8d`. Coverage includes GPUParticles3D skin replacement/clear, GPUParticles2D texture replacement/clear, CPUParticles2D texture replacement/clear, CPUParticles3D mesh replacement/clear, and 2D/3D ParticleProcessMaterial emission-shape signal replacement. Both component receipts and the queue manifest are refreshed; this remains resource-sync coverage, not full GPU qualification.

## ParticleProcessMaterial atomic parameter bounds — 2026-11-02

`ParticleProcessMaterial::set_param(Vector2)` now validates both min/max components before updating either side. A NaN/Inf in either component therefore preserves the complete previous parameter range instead of partially publishing the first component. The source fixture is HEAD RED and current GREEN at 8/8; the tests-enabled `[Particles]*` selection passes 14 cases / 65 assertions, all 36 source fixtures pass, and the 230-frame Metal Forward+ lifecycle smoke remains green on binary SHA-256 `f45689b8be877b8d0a56d09d557df63f890e44c359b1fc3170a95f3b1a4a1ec9`.

Evidence: `work/particle-process-material-atomic-bounds-20261102/RECEIPT.md` and the refreshed `work/particle-runtime-smoke-final-20261102/RECEIPT.json`. This is a bounded setter-atomicity fix; GPU allocation-failure qualification, typed-close, G1 and ANGE owner integration remain separate active rows.

## Physics callback-retirement current-binary replay after particle atomicity build — 2026-11-02

The current tests-enabled Editor was replayed with the explicit backend/thread environment required by the physics fixtures. All 12 serialized GodotPhysics2D, GodotPhysics3D and Jolt callback-epoch/native-retirement commands returned rc=0 on binary SHA-256 `f45689b8be877b8d0a56d09d557df63f890e44c359b1fc3170a95f3b1a4a1ec9`. Evidence is in `work/physics-query-callback-epoch-20261102/current-binary-rerun-f45689.json` and the refreshed physics receipt; the earlier missing-environment invocation is retained only as a command diagnostic and is not counted as verification.

## Optional RD upload-retry host capability check — 2026-11-02

The opt-in `[ParticlesProcessRetryGPU]*` production upload-retry case was attempted with `GODOT_TEST_PARTICLES_PROCESS_RD=1`. The headless macOS test host has no initialized `RenderingDevice/RendererCompositorRD`, so the case fails at its explicit capability check; this is recorded as an environment limitation and does not alter the source-level retry evidence or claim a Vulkan production-path pass.

## Particle renderer input-contract evidence expansion — 2026-11-02

The main particle receipt now includes the existing GREEN source checks for RD Canvas SDF collision inputs (`Transform2D`/`Rect2`) and RD view-axis inputs. Both checks pass; the bounded source scope is now 40 files with 14 source-contract checks, while the 36-fixture matrix and runtime claims remain unchanged.

## RD renderer setter contract fixture — 2026-11-02

Added `particle_renderer_setter_contract`, a RED→GREEN source fixture for the ten direct RD particle setter boundaries: custom AABB, fixed FPS, draw passes, interpolation, collision base size, emission transform, emitter velocity, trail bind poses, Canvas SDF inputs and view axes. HEAD is RED and current source is GREEN at 10/10; the complete source-fixture matrix now passes 37/37. The current binary/runtime claims are unchanged.

## Particle resource sync contract fixture — 2026-11-02

Added `particle_resource_sync_contract`, a RED→GREEN source fixture for six replacement/clear paths: CPUParticles3D mesh, CPUParticles2D texture, GPUParticles2D texture, GPUParticles2D process material, GPUParticles3D skin and GPUParticles3D process material. Current source is GREEN at 6/6; the complete source-fixture matrix now passes 38/38. Native resource-sync runtime evidence remains in the tests-enabled particle receipt.

## GLES3 particle process-failure retry parity — 2026-11-02

GLES3 `_particles_process()` now reports failure and restores phase/cycle/frame/clear/emitting state when material or shader setup fails. `update_particles()` preserves pre-process/request remainders, retries failed fixed-frame work on the next update pass, and only clears request state after successful processing. The new `particle_gles3_process_retry` fixture is HEAD RED `0/8`, current GREEN `8/8`; the tests-enabled Editor build passes all six particle selections, `[ParticlesProcessRetry]*` 3/45, the 230-frame Metal smoke, and the complete 39-fixture source matrix on binary SHA-256 `f532227e10e8cbcdd1294ea78e184708e53545e79cb1fe8d6a77e864a2e78a7d`.

Evidence: `work/particle-gles3-process-retry-20261102/RECEIPT.md` and refreshed `work/particle-runtime-smoke-final-20261102/RECEIPT.json`. Live Compatibility GPU allocation-failure qualification remains a separate capability-limited item.

The same fixture now also checks `_particles_free_data()` clears retry-pending, frame-delta-committed and preprocess-pending markers; current source is GREEN at 11/11 and the complete source matrix remains 39/39.

Active Reload direct capture was replayed on the same current GLES3 retry binary (`f532227e10e8cbcdd1294ea78e184708e53545e79cb1fe8d6a77e864a2e78a7d`): 1 case / 33 assertions and the complete `device_exit::method/*` selection at 10 cases / 515 assertions passed.

## GLES3 Compatibility lifecycle smoke — 2026-11-02

The current tests-enabled Editor completed the existing 230-frame particle lifecycle project with the macOS OpenGL3 Compatibility renderer. All stages passed: active/restarted/resumed each rendered 690 red pixels, stopped/freed rendered zero, AABB spread shrank and cleared, queue-free reported `invalid=true`, and the process exited rc=0. Evidence: `work/particle-gles3-runtime-smoke-20261102/RECEIPT.md` and `run-result.json` on binary SHA-256 `f532227e10e8cbcdd1294ea78e184708e53545e79cb1fe8d6a77e864a2e78a7d`. Forced allocation-failure injection remains capability-limited.

## K02 M12 target-frame marker matrix — 2026-11-02

The fixed-build real Metal successor completed all 64 Editor/template-release, single/double, Mobile/Forward+ simulation and depth/shadow cells. Result inspection checked 19,440 literal samples, 304 PNGs, 32 release PCKs and exact 18-coordinate failure sets in all 24 negative controls. The stage marker advances in each captured target frame, including negative controls. Evidence: `work/k02-target-stage-marker-20261102/RECEIPT.md` and `MATRIX.json`. This closes the target-image freshness check for this fixed M12 matrix only; raw depth, scheduling, physical retirement, product owner, typed close and G1 retain their own acceptance rows.

## ParticleProcessMaterial 3D scale axis bounds — 2026-11-02

`set_scale_3d_min/max` now reconcile the opposite bound per axis, so changing one coordinate cannot leave another coordinate inverted. The native regression was RED at 2/4 assertions before the fix, then GREEN at 4/4 after a tests-enabled Editor rebuild; the `[Particles]*` neighbor group passed 27 cases / 146 assertions. The focused test was rerun independently on the recorded binary with rc=0. Evidence: `work/particle-process-material-scale-axis-20261102/RECEIPT.json`. This is bounded material setter behavior, not GPU or product qualification.

## ParticleProcessMaterial 3D rotation axis bounds — 2026-11-02

`set_rotation_3d_min/max` and `set_rotation_velocity_3d_min/max` now reconcile the opposite bound independently on each axis. The focused rotation and rotation-velocity regressions were each RED at 2/4 assertions before the change, then GREEN at 4/4 after the tests-enabled Editor rebuild; the `[Particles]*` neighbor group passed 29 cases / 154 assertions. Evidence: `work/particle-process-material-rotation-axis-20261102/RECEIPT.json`. This is bounded material setter behavior, not GPU or product qualification.

## R02 multi-readback copy identity coverage — 2026-11-02

The existing internal V2 aggregate-copy test now changes a completed later range's request serial and backing serial separately. Each mismatch returns busy, writes zero bytes and leaves the full 44-byte destination untouched; restoring the original identities copies the exact expected bytes. The tests-enabled Editor rebuild, focused case (37/37 assertions) and `[Qualification][MultiReadback]*` neighbors (8 cases / 317 assertions) passed. Evidence: `work/qualification-multireadback-origin-20261102/RECEIPT.json`. This adds regression coverage to the Godot ticket reducer; it is not a real ANGE request or GPU six-link receipt.

## GLES3 heightfield extents synchronization — 2026-11-02

Compatibility heightfield collision storage now retires its cached depth texture and framebuffer when X/Z extents change, allowing the next access to rebuild the framebuffer with the new aspect ratio. The RED-to-GREEN source fixture passes 8/8 checks. A rebuilt tests-enabled Editor passed `[Particles]*` 29/154, `[GPUParticles]*` 19/101 and `[ParticlesProcessRetry]*` 3/47; the existing 230-frame Compatibility lifecycle smoke also passed on binary SHA-256 `4e05dc4c876764a48338a961f9cd79a755af0f088744945152beec50ac9c2b98`. This is a bounded GLES3 resource-invalidation fix; live heightfield mutation and full GLES3 qualification remain separate acceptance items. Evidence: `work/particle-gles3-heightfield-extents-sync-20261102/RECEIPT.md`.

## Particle point-emission count lower bound — 2026-11-02

`ParticleProcessMaterial::set_emission_point_count()` now rejects zero and the editor property range starts at one, preventing the generated point-emission shader from calculating a negative texel index. The new RED test failed before the change (0 published instead of retaining 8); after the change the focused setter case passed 1 case / 2 assertions and `[Particles]*` passed 29 cases / 154 assertions. The expanded source fixture passes 10/10 while its HEAD baseline remains RED. Binary SHA-256: `a0ff3d29be86d1201760163474f38a183c4f7bbe3aa60e5eb95d15c824051cf8`. Evidence: `work/particle-emission-point-count-20261102/RECEIPT.md`.

## R02 native correlation acceptance table — 2026-11-02

Recorded the existing Godot V2 request/range/report/fact fields, A E09 consumer call sites, exact copy preconditions and release/drain constraints. Formal production issuer, owner/correlation source, result decoder/schema, paired SDK/DSO tuple and same-request runtime evidence remain explicitly pending; no fixture identity is promoted. Evidence: `work/particle-native-six-link-audit-20261102/R02_CORRELATION_ACCEPTANCE_TABLE.md`.

## CPU empty point emission — 2026-11-02

2D/3D empty point arrays deactivate and skip new particles. Source RED/GREEN and native neighbor suites passed. Receipt: `work/particle-cpu-empty-points-20261102/RECEIPT.md`.

## RD particle cleanup guards — 2026-11-02

Direct native particle/collision free rejects null owner lookup. Source RED/GREEN, editor build and neighbor tests passed; no live invalid-RID GPU test claimed. Receipt: `work/particle-rd-cleanup-guards-20261102/RECEIPT.md`.

## RD heightfield extents cache invalidation — 2026-11-02

RD particle heightfield collision storage now retires cached framebuffer and depth texture when X/Z extents change, preserving cache for Y-only/equal/non-heightfield updates and rejecting invalid extents before mutation. The source fixture is RED at baseline (6/11) and GREEN currently (11/11); editor build and `[Particles]*` / `[ParticlesProcessRetry]*` neighbors pass. This is bounded native RD storage coverage; live GPU allocation-failure qualification remains separate. Receipt: `work/particle-rd-heightfield-extents-20261102/RECEIPT.md`.

## Native six-link capture harness — 2026-11-02

Added a pinned process-capture control and 17-test regression suite. It records editor/SDK/DSO/request/oracle/issuer/project hashes, exact command, raw logs, exit/timeout/drift state, and refuses unsupported or unpinned inputs. It never promotes fixture output to production acceptance. Evidence: `work/particle-native-six-link-audit-20261102/RECEIPT.md`; production issuer and same-ticket native completion/readback/release/drain remain pending.

## GPU process animation warning refresh — 2026-11-03

`ParticleProcessMaterial` now notifies `GPUParticles2D/3D` when animation speed, offset or their textures change in place. The nodes refresh edited-scene warnings and detach from replaced or cleared materials. Native SceneTree RED 0/2 → GREEN 14/14 assertions; neighbor `[GPUParticles]*` 22/118 and `[Particles]*` 31/161 passed. The test build was restored to the fixed editor afterward. Evidence: `work/particle-process-animation-warning-sync-20261103/RECEIPT.md`. Product issuer and GPU six-link acceptance remain active independently.

## Compatibility 3D particle field admission — 2026-11-03

GLES3 particle processing now excludes unsupported 3D SDF/vector-field collisions before frame-parameter counts advance. The extracted production loop is HEAD RED (3/4 counts) and current GREEN (2/3); source-matched Editor build and particle neighbors passed. A real OpenGL 4.1 Compatibility scene containing both unsupported fields and supported sphere controls ran 60 frames with nonempty particle data, finite AABB and the expected warning, exiting rc 0. Evidence: `work/particle-field-texture-rebind-20261103/RECEIPT.md` and `gles3-unsupported-live.*`. Live GLES3 frame counters and field pixel/motion semantics remain unobserved; product six-link remains active.

## Continuation — Godot V2 ABI synchronization package, 2026-11-03

Godot native bridge V2 declarations and registration were rechecked at HEAD `f3cacf1ceed115e796f43cea804ff9dfad597b18`. A byte-for-byte generated-header package is available at `work/godot-v2-abi-sync-20261103/`, with C/C++ layout smoke coverage and exact source/fixture hashes. It records the three V2 entry points (`prepare_uploads_v2`, `record_prepared_v2`, `observe_v2`) and same-ticket V1 upload/facts/copy/cancel/release pairing, including the exact 1-or-3 readback validator constraints. Package delivery to the authorized A/C and main sessions is complete. This closes ABI synchronization evidence only; original ANGE issuer/consumer production integration, SDK/DSO pair, and six-link acceptance remain open in their owner lanes.

## 2026-11-03 GPU damping zero-velocity boundary

A real Vulkan Mobile readback probe exercised ParticleProcessMaterial damping with zero initial velocity in both linear and damping-as-friction modes plus a moving control. The fixed editor produced five active particles per case, finite velocities, zero velocity for both zero-input cases, and nonzero control velocity (rc 0). OpenGL Compatibility exposed a different 1024-byte buffer shape with no active particles and is recorded unsupported for this probe. No shader change is warranted. Evidence: `work/particle-damping-zero-20261103/RECEIPT.md` and `probe.gd`.

## 2026-11-03 fixed-editor particle regression refresh

After restoring the fixed editor, `[ParticlesReadback]*` passed 11/11 cases and 58/58 assertions; `[ParticlesProcessRetry]*` passed 3/3 cases and 47/47 assertions. The attempted unsupported `--tests --filter` invocation was terminated without a result and is not counted. The original Method issuer remains absent; the exact R02/K01 owner join requirements are in `work/particle-native-six-link-audit-20261102/R02_K01_PRODUCTION_JOIN_HANDOFF_20261103.md`.

## 2026-11-03 — current editor runtime closure of three particle fixes

Editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4` now verifies CPU2D interpolation notifications (runtime 13/13; editor gizmo 6/6), scale-over-velocity minimum offset (Vulkan Mobile 15/15 active particle scale samples, rc 0), and mirrored heightfield follow notification returns `(2, 0, 2)` without hanging. Particle neighbors pass 72/72 cases, 381/381 assertions. Exact CPU2D and scale receipts are under their `work/particle-*-20261103/RECEIPT.md` directories. Heightfield still has only a narrow SceneTree smoke plus the existing 20-case source C++ suite, not full GPU acceptance. System scalar E09 matching runtime and production six-link gates remain open independently.

## 2026-11-03 — heightfield native notification RED/GREEN

The actual SceneTree follow-camera matrix now covers 12 exact-coordinate cases. Retained old editor passed the first four and timed out on the first mirrored scale; current `5feadf94...` editor passed all 12 with rc 0. Before/after hashes and raw logs are preserved in `work/particle-heightfield-camera-follow-20261103/native-{red,green}.{json,log}`. Full rendered collision behavior remains a separate gate.

## 2026-11-03 — live heightfield extent allocation and material gizmo follow-up

On fixed editor `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`, the live heightfield X/Z mutation matrix passes 6/6 on both OpenGL Compatibility and Vulkan Mobile, including Y-only stability, transposed aspect, restoration and baseline return after free. The final exact 2:1 allocation oracle accounts for RD's driver allocation size; an earlier fixed-byte oracle failure is retained and is not a production RED/GREEN. Evidence: `work/particle-gles3-heightfield-extents-sync-20261102/RECEIPT.md` and `work/particle-rd-heightfield-extents-20261102/RECEIPT.md`. This observes resource accounting, not framebuffer contents or particle collision motion.

The independent actual editor `EditorNode3DGizmo._redraw` control passes 9/9 checks for GPUParticles3D process-material assignment, in-place change, replacement detachment and clear detachment. Evidence: `work/particle-gpu3d-material-gizmo-runtime-20261103/RECEIPT.md`. It observes callbacks and shapes, not built-in gizmo geometry/pixels. System scalar E09 matching runtime and production six-link remain open; the long goal stays active.

## 2026-11-03 — matching editor CoreBuildSelection delivery

After main's first Metal run stopped at `loader.compiled_identity`, D extracted the current fixed editor's actual embedded identity, checked its exact agreement with the adjacent controlled-build sidecar, and validated the closed selection with ANGE's real `load_core_selection` consumer. Editor SHA remained `5feadf94...`; the delivered record is `work/system-scalar-native-20261103/core-selection/CoreBuildSelection.schema1.json` (SHA-256 `7b86787c577d3224b93ead1e0a808ab1e52b3f9eb3c0be163dca52520d578859`). Main owns the unique matching DSO build and runtime. This closes the missing build-selection handoff only; E09 matching GPU runtime and production six-link remain open.

## 2026-11-03 — trail runtime mutation and initial velocity contract audit

The fixed editor ran a real Metal Forward+ GPUParticles3D trail scene through enable/disable/reenable and material trail-flag mutation. The run exited rc 0 with active/reenabled/restarted red-pixel counts 2048, disabled 9402, flag-off 7680, and flag reenabled 2048; the override identity stayed stable and the captured AABB stayed finite. Evidence: `work/particle-trail-runtime-followup-20261103/RECEIPT.json` and PNG captures. This proves bounded live trail mutation behavior, not trail pixel correctness or all renderers.

A read-only contract audit of `PARAM_INITIAL_LINEAR_VELOCITY` found the public getter/setter and texture documentation, but no specified GPU sampling semantics, no deprecation marker, and no matching decision in the formal particle design docs. Current GPU code stores and signals the texture but intentionally publishes no sampler RID; CPU conversion consumes its curve independently. A production fix cannot yet be selected uniquely from the available contract: sampling stage, coordinates, value meaning and CPU/GPU behavior remain unspecified. Evidence: `work/particle-initial-velocity-contract-followup-20261103/REVIEW.md`.

## 2026-11-03 — Vulkan Mobile trail runtime follow-up

The fixed editor ran a real Vulkan Mobile GPUParticles3D trail scene through node trail disable/reenable, material trail-flag off/on and restart with one retained red StandardMaterial3D override. The run exited rc 0; red-pixel counts were `2048 / 10735 / 2048 / 7680 / 2048 / 2048` across active, disabled, reenabled, flag-off, flag-on and restart. Override identity remained stable and all runner checks passed. Evidence: `work/particle-trail-vulkan-followup-20261103/RECEIPT.md`, raw log, result JSON and PNG hashes. This is renderer-local footprint reversibility; exact trail history/vertices and product six-link acceptance remain separate.

## 2026-11-03 — CPU empty points rendered runtime

On fixed editor `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`, a real Metal Forward+ SceneTree probe passed all 12 framebuffer stages for CPU2D/3D POINTS and DIRECTED_POINTS. Each family transitions empty points → one-point control → empty points restored, with red pixels `0 / 16 / 0` in 2D and `0 / 900 / 0` in 3D. Each capture follows restart and a 0.7-second wait against lifetime 0.2. Exit rc 0, editor hash unchanged. Evidence: `work/particle-cpu-empty-points-runtime-20261103/RECEIPT.md`, exact command/result JSON, raw log and 12 PNGs. This verifies visible empty-point behavior, not active counts or mismatched/zero-normal semantics.

## 2026-11-03 — CPU manual process request live lifecycle

A headless Dummy SceneTree probe exercised production CPUParticles2D/3D `request_particles_process(0.3, 0.1)` on the fixed editor. Each node passed six checks: stopped before request, internal processing activated, residual left emission off, one `finished` signal, then processing stopped. Coordinator independently reran the exact command with rc 0 and `RESULT checks=12 failures=0 finished=1`; editor SHA-256 stayed `5feadf94...`. Evidence: `work/particle-cpu-manual-request-live-20261103/RECEIPT.md` and `tests/scene/fixtures/cpu_particle_manual_request_live/probe.gd`. This observes public lifecycle signals, not per-particle birth counts or GPU pixels.

## 2026-11-03 — CPU directed point normals live boundary

The fixed editor ran a real Metal Forward+ probe for CPU2D/3D directed point emission with empty, short, long, matched zero and matched nonzero normals arrays. All 10 stages had a visible finite-centroid red footprint, rc 0, unchanged editor SHA. Empty/short/long PNG hashes matched within each dimension, consistent with source's `normals.size() == point_count` gate; matched zero controls remained visible. Evidence: `work/particle-cpu-directed-normals-runtime-20261103/RECEIPT.md`, raw log, result JSON and ten PNGs. Two initial GDScript parse failures were probe setup failures and are retained separately. These pictures do not assert internal per-particle velocity/transform finiteness.

## 2026-11-03 — direct GLES3 2D transform readback

On the fixed editor, a real OpenGL 4.1 Compatibility direct 2D particle RID returned 384 bytes for eight 48-byte slots. A live probe decoded finite basis axes and translations for active slots, then matched the extrema against `particles_get_current_aabb()` in X and Y within 0.001. Final run rc 0 with six runner checks true and unchanged editor SHA. Evidence: `work/particle-gles3-2d-layout-followup-20261103/RECEIPT.md`, exact result JSON and raw log. This closes the direct 2D layout-offset observation left open in the earlier GLES3 AABB receipt, without claiming rendered pixels or allocation-failure injection.

## 2026-11-03 — CPU directed normal setter rejection

A fixed-editor headless SceneTree probe exercised the public CPUParticles2D/3D normal setters with a valid baseline, a two-element replacement containing NaN (2D) or infinity (3D), and a later valid replacement. Both rejected arrays preserved the baseline and both valid replacements succeeded: six probe checks, two expected diagnostics, rc 0, unchanged editor SHA. Evidence: `work/particle-cpu-normal-setter-live-20261103/RECEIPT.md`, exact result JSON and raw log. This covers atomic setter behavior, not internal particle transforms.

## 2026-11-03 — CPU zero-normal finite transform assertions syntax-verified

The existing `[CPUParticles] directed point zero normal produces zero initial velocity` native test now also checks that the first 2D transform and first 3D basis remain finite after processing. This extends the prior velocity-only guard without changing production code. Using the installed Xcode 27.2 SDK path, the modified `tests/scene/test_gpu_particles.cpp` passed a complete `clang++ -fsyntax-only` translation-unit check with the current compile flags. A tests-enabled editor rebuild and focused runtime result are still pending; no GREEN runtime claim is made.
The exact syntax command, source SHA and compiler output are recorded in `work/particle-cpu-zero-normal-finite-20261103/RECEIPT.md` and `result.json`.

## 2026-11-03 — GPUParticles3D sub-emitter visible lifecycle

The fixed editor ran a real Vulkan Mobile scene with a red source and nonemitting green target. Target green pixels followed source attach → detach → reattach as `0 / 384 / 0 / 384` across baseline, attached, detached and restored stages. The process exited rc 0, all nine runner checks passed, and editor SHA remained `5feadf94...`. Evidence: `work/particle-sub-emitter-pixels-20261103/RECEIPT.md`, exact command/result JSON, raw log and four PNGs. This closes the previously open sub-emitted pixel observation for this renderer; exact sub-event count and product six-link remain separate.

## 2026-11-03 — GPUParticles3D sub-emitter target replacement pixels

With the source node retained, a real Vulkan Mobile probe removed the green `Target` and then added a same-name blue replacement. Green/blue pixels were `0/0` at baseline, `384/0` with the old target, `0/0` after removal, and `0/384` after replacement. Final rc 0, all 12 checks passed, editor SHA unchanged. Evidence: `work/particle-sub-emitter-replacement-pixels-20261103/RECEIPT.md`, result JSON, raw log and four PNGs. This extends visible sub-emitter rebinding coverage to replacement, separate from exact event counts and product six-link acceptance.

## 2026-11-03 — GPUParticles3D amount-ratio footprint

On the fixed editor, a real Vulkan Mobile scene with fixed seed and spread motion changed `amount_ratio` 1.0 → 0.0 → 0.5 → 1.0. The final framebuffer red-pixel counts were `1033 / 0 / 466 / 1012`; an exploratory run retained the same ordering. Final rc 0, nine runner checks passed, editor SHA unchanged. Evidence: `work/particle-amount-ratio-runtime-20261103/RECEIPT.md`, exact command/result JSON, raw log and four PNGs. This covers visible footprint response, not exact active counts or a numerical half-count guarantee.

## 2026-11-03 — GPUParticles2D amount-ratio footprint

On the fixed editor, a real Vulkan Mobile scene with fixed seed and spread motion changed `amount_ratio` 1.0 → 0.0 → 0.5 → 1.0. The final red-pixel counts were `3193 / 0 / 1302 / 2948`; all ten runner checks passed, rc 0, and editor SHA remained unchanged. Evidence: `work/particle-amount-ratio-2d-runtime-20261103/RECEIPT.md`, exact result JSON, raw log and four PNGs. This covers visible 2D footprint response, not exact active counts or numerical half-count proportionality.

## 2026-11-03 — CPU manual request invalid-input preservation live

The fixed editor headless SceneTree fixture now sends negative and non-finite primary and residual `request_particles_process()` calls to already-processing CPUParticles2D and CPUParticles3D nodes. All eight invalid calls are rejected with the expected finite/non-negative diagnostic; processing remains active, emission remains off, and each node still emits `finished` once before stopping. Final run: 28/28 checks, rc 0, editor SHA unchanged. Evidence: `work/particle-cpu-manual-request-live-20261103/RECEIPT.md`, `invalid-request-run.log`, `invalid-request-result.json`, and `tests/scene/fixtures/cpu_particle_manual_request_live/probe.gd`. This closes the live invalid-input state-preservation boundary only; GPU, exact particle counts, and product six-link acceptance remain separate.

## 2026-11-04 — GPUParticles2D sub-emitter visible lifecycle

A fixed-editor Vulkan Mobile probe exercised GPUParticles2D source attach → detach → reattach against a nonemitting green target. Green pixels were `0 / 560 / 0 / 560`; rc 0, nine checks passed, editor SHA unchanged, and four PNG captures plus raw log/result JSON were retained. This adds 2D renderer-local visible lifecycle evidence; exact sub-event counts, other renderers and product six-link acceptance remain separate.

## 2026-11-04 — GPUParticles2D sub-emitter target replacement pixels

A fixed-editor Vulkan Mobile probe retained one GPUParticles2D source while removing its green target and adding a same-name blue target. Green/blue pixels were `0/0`, `560/0`, `0/0`, and `0/576` across baseline, old target, removal and replacement. Final rc 0, all 12 runner checks passed, editor SHA unchanged. Evidence: `work/particle-sub-emitter-2d-replacement-20261104/RECEIPT.md`, raw log, result JSON and four PNGs. This closes renderer-local visible 2D replacement behavior; exact event counts and product six-link acceptance remain separate.

## 2026-11-04 — CPU zero-normal finite transform runtime audit

The fixed editor ran the focused `[CPUParticles] directed point zero normal produces zero initial velocity` doctest with 1 case / 2 assertions, rc 0. Source/line inspection shows this binary predates the newly added finite transform/basis assertions, so the run confirms only the two pre-existing zero-velocity checks. The new finite assertions remain syntax-verified only pending a matching tests-enabled editor rebuild. Evidence: `work/particle-cpu-zero-normal-finite-20261103/RECEIPT.md` and `runtime-attempt.log`.

An independent fixed-editor Vulkan Mobile public-buffer probe read a finite nonzero 3D CPU particle transform after zero-normal directed emission; headless Dummy returned no buffer, and CPUParticles2D exposes no public base RID. This is partial runtime evidence only; the native finite assertions still await a matching tests-enabled editor build. Evidence: `work/particle-cpu-zero-normal-live-20261104/RECEIPT.md`.

## 2026-11-04 — GPUParticles2D continuous emission resume fix

Source audit found and fixed a 2D/3D parity gap: after a non-one-shot GPUParticles2D is stopped with `emitting=false`, setting `emitting=true` did not restore internal processing; GPUParticles3D already did. The fixed-editor RED probe recorded 2D immediate/next-frame resume failures while the 3D control passed. The minimal 2D branch now calls `set_process_internal(true)` on re-emission. Current `scene/2d/gpu_particles_2d.cpp` passes full translation-unit syntax verification with Xcode 27.2 SDK, rc 0. A matching editor rebuild and post-fix runtime GREEN remain pending. Evidence: `work/particle-gpu2d-continuous-resume-20261104/RECEIPT.md`, `red.log`, `syntax-result.json`.

The focused native regression now includes paired 2D/3D SceneTree assertions for continuous re-emission restoring internal processing. The modified test translation unit passes syntax verification; runtime GREEN remains pending a matching editor rebuild.

An isolated suffix-specific tests-enabled editor attempt was stopped before link because SCons required a full object rebuild under about 1.0 GiB free; the fixed editor remained unchanged. The 2D resume runtime GREEN gate therefore remains open.

## 2026-11-04 — GPUParticles2D continuous emission visible lifecycle

A fixed-editor Vulkan Mobile probe exercised non-one-shot GPUParticles2D emission on → drain → emission on. Red pixels were `3712 / 0 / 2947`; rc 0, 10 checks passed, editor SHA unchanged. The fixed editor reported `internal_active=false` immediately after resume yet visible rendering recovered, so the tentative 2D internal-process parity patch was reverted; no production change remains from that proposal. Evidence: `work/particle-gpu2d-continuous-visible-20261104/RECEIPT.md`, result JSON, raw log and PNGs.

The preceding GPUParticles2D internal-processing parity proposal and its pending rebuild gate are superseded by the visible lifecycle result above. The production/test edits for that proposal were reverted; only the renderer-local stop/drain/resume receipt remains active.

## 2026-11-04 — GPUParticles3D draw pass Mesh replacement connections

A fixed-editor headless SceneTree probe exercised two passes sharing an old Mesh, replacement of one pass, pass count shrink, then final clear. Six checks passed, rc 0: old Mesh retained its signal connection until its last pass was removed, new Mesh stayed connected while referenced, and final clear disconnected it. Evidence: `work/particle-gpu3d-draw-pass-mesh-live-20261104/RECEIPT.md`, `result.json`, raw log and probe. This extends the existing native shared-Mesh test with the replacement/shrink interleaving; it is resource-signal lifecycle evidence only.

## 2026-11-04 — GPUParticles2D texture mutation and replacement visibility

A fixed-editor Vulkan Mobile probe rendered GPUParticles2D texture stages initial red → in-place green update → blue replacement → detached old texture mutation. Each stage produced 256 pixels of the expected active color; final rc 0, 12 checks passed, editor SHA unchanged, no engine/script errors. Evidence: `work/particle-gpu2d-texture-visible-20261104/RECEIPT.md`, result JSON, raw log and four PNGs. This is renderer-local resource synchronization evidence only.

## 2026-11-04 — GPUParticles3D Mesh mutation and replacement visibility

A fixed-editor Vulkan Mobile probe changed a retained GPUParticles3D draw-pass Mesh in place, replaced it, then modified the detached old Mesh. Red framebuffer pixels were `100 → 784 → 256 → 256`; the final two PNGs were identical, rc 0, and no engine/script errors occurred. Evidence: `work/particle-gpu3d-mesh-visible-20261104/RECEIPT.md`, result JSON and captures. This is renderer-local draw-pass resource synchronization evidence.

## 2026-11-04 — GLES3 direct 3D transform readback

The fixed editor ran a real OpenGL 4.1 Compatibility direct 3D particle RID for 90 frames. Readback returned 512 bytes for eight 64-byte slots; two active slots had finite basis/origin values, and decoded extrema matched `particles_get_current_aabb()` within 0.001 on all axes. Final rc 0, editor SHA unchanged. Evidence: `work/particle-gles3-3d-layout-followup-20261104/RECEIPT.md`, `result.json` and `run.log`. This closes ordinary 3D layout/AABB observation only; pixels, forced allocation failure and product six-link remain separate.

## 2026-11-04 — GPUParticles2D Compatibility texture visibility

A fixed-editor OpenGL 4.1 Compatibility scene exercised GPUParticles2D texture update in place, replacement, and old-resource mutation after detachment. Captured red/green/blue pixels were `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256`; the last two PNGs matched, rc 0, 12 checks passed, editor SHA unchanged. Evidence: `work/particle-gles3-2d-texture-visible-20261104/RECEIPT.md`, result JSON, raw log and four PNGs. This closes the visible 2D texture boundary for this renderer; heightfield collision motion and product six-link remain active separately.

## 2026-11-04 — heightfield collision motion exploratory readback

A Vulkan Mobile probe established the RD particle transform's Y translation offset and added same-seed disabled controls plus a sphere positive control. The sphere changed the particle Y result from `-0.786667` to `1.010000`, proving the collision/readback path. Heightfield and disabled controls remained identical at both widths, so heightfield admission/update remains unaccepted. Exact corrected, paired and positive-control output is in `work/particle-heightfield-motion-20261104/RECEIPT.md`; this remains the next runtime gate, not a product six-link result.
A follow-up with `UPDATE_MODE_ALWAYS` still produced heightfield/control equality while the sphere control remained green. The unresolved boundary is now narrowed to heightfield framebuffer content or collider admission; no production change was made.
Replacing the flat plane with a thick BoxMesh also preserved the equality (`box-geometry.log`), further narrowing the fixture gap to generated heightfield depth content or sampling.
An explicit heightfield mask 1 plus 30-frame render warmup also preserved equality (`mask-warmup.log`). Source confirms a masked geometry query exists, but its returned instance count and depth contents remain unobserved; the receipt was corrected to avoid claiming geometry submission.

The same fixed editor and `--fixed-fps 60` probe split the renderers: Forward+ heightfield width 8 stopped falling particles at Y `0.11` versus disabled Y `-7.43`, while Mobile left both at `-7.43`; sphere controls worked on both. Mobile's heightfield pass used default framebuffer load flags, whereas Forward+ explicitly cleared depth. The one-line Mobile clear change passed its RED→GREEN source check, clean diff and translation-unit syntax rc 0. A source-matched tests-enabled editor rebuilt rc 0 (SHA `2eac37a21ac2377fe6ae3aab9a9d7ff2d7db59019f8cbfc351d4065f17ad8b2f`), and its Vulkan Mobile replay produced collision Y `0.11` versus disabled `-7.43` at width 8, with width 2 negative control and sphere positive control. The original fixed editor SHA was restored afterward. Evidence: `work/particle-mobile-heightfield-clear-20261104/RECEIPT.md` and `work/particle-heightfield-motion-20261104/{forward-plus,mobile-fixed-fps}.log`.

The same adapted probe on OpenGL 4.1 Compatibility produced a live heightfield split: width 8 collision Y `0.11` versus disabled Y `-7.65`, while width 2 remained an outside-field negative control; the sphere positive control stopped at `1.01`. rc 0, 512-byte buffers, eight active finite records. Evidence: `work/particle-gles3-heightfield-motion-20261104/RECEIPT.md` and `run.log`.

The pending CPU zero-normal native assertions have now run on a source-matched tests-enabled editor (SHA `2eac37a21ac2377fe6ae3aab9a9d7ff2d7db59019f8cbfc351d4065f17ad8b2f`): focused case **4/4 assertions** and `[CPUParticles]*` neighbors **16 cases / 84 assertions**, both rc 0. The fixed editor was restored afterward. Evidence: `work/particle-cpu-zero-normal-finite-20261103/RECEIPT.md`, `focused-current.log`, and `neighbor-current.log`.

The fixed editor's OpenGL 4.1 Compatibility GPUParticles3D Mesh probe observed red pixels `100 → 784 → 256 → 256` for original Mesh, in-place enlargement, replacement and mutation of the detached old Mesh. The final two captures have identical hashes; rc 0 and no engine/script errors. This adds renderer-local visible resource-sync coverage. Evidence: `work/particle-gles3-gpu3d-mesh-visible-20261104/RECEIPT.md`, `result.json`, `run.log` and four PNGs.

The fixed editor's OpenGL 4.1 Compatibility GPUParticles2D amount-ratio probe observed red pixels `3700 → 0 → 1753 → 3700` for ratios `1.0 → 0.0 → 0.5 → 1.0`; rc 0, all images saved and no engine/script errors. This adds visible amount-ratio lifecycle coverage for Compatibility. Evidence: `work/particle-gles3-amount-ratio-2d-20261105/RECEIPT.md` and `run.log`.

The fixed editor's OpenGL 4.1 Compatibility GPUParticles3D amount-ratio probe observed red pixels `1307 → 0 → 628 → 1305` for ratios `1.0 → 0.0 → 0.5 → 1.0`; rc 0, all images saved and no engine/script errors. This adds visible 3D amount-ratio lifecycle coverage for Compatibility. Evidence: `work/particle-gles3-amount-ratio-3d-20261105/RECEIPT.md` and `run.log`.

The fixed editor's OpenGL 4.1 Compatibility CPUParticles2D texture probe observed red/green/blue pixels `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256` across in-place update, replacement and detached-old mutation; rc 0, all captures saved and no engine/script errors. Evidence: `work/particle-cpu2d-texture-visible-20261105/RECEIPT.md` and `run.log`.

The same CPUParticles2D texture lifecycle on Vulkan Mobile produced identical color stage counts `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256`; final PNG hashes matched, rc 0 and no engine/script errors. Evidence: `work/particle-cpu2d-texture-vulkan-20261105/RECEIPT.md`, `result.json` and `run.log`.

The fixed editor's Vulkan Mobile CPUParticles3D Mesh probe observed red pixels `100 → 784 → 256 → 256` for original Mesh, in-place enlargement, replacement and detached-old mutation. The final two captures match; rc 0 and no engine/script errors. Evidence: `work/particle-cpu3d-mesh-visible-20261105/RECEIPT.md`, `result.json` and `run.log`.

The same CPUParticles3D Mesh lifecycle on OpenGL 4.1 Compatibility produced `100 → 784 → 256 → 256`, with matching final image hashes, rc 0 and no engine/script errors. Evidence: `work/particle-cpu3d-mesh-gles3-20261105/RECEIPT.md`, `result.json` and `run.log`.

The fixed editor's OpenGL 4.1 Compatibility scale-over-velocity probe read 21 active GPUParticles3D scales at speeds 2/3/4: `1.0`, `1.9941407442`, `2.9997649193`, each within 0.03 of the expected 1/2/3. The corrected 64-byte Compatibility layout run returned rc 0 with no engine/script errors. Evidence: `work/particle-gles3-scale-over-velocity-20261105/RECEIPT.md`, `result.json` and `run.log`.

The attempted Compatibility GPUParticles2D sub-emitter replacement probe emitted the renderer's explicit unsupported warnings and produced zero target pixels at all stages. It is recorded as a renderer capability boundary, not a regression or acceptance. Evidence: `work/particle-gles3-sub-emitter-2d-replacement-20261105/run.log`.

The fixed editor Compatibility probe for scale-over-velocity equal bounds and near-zero speed returned six finite active records in each of six cases. Equal bounds sampled scale 1.0 at/below the reference velocity and 2.99976491928101 above it; zero/near-zero sampled 1.0; the moving control sampled 1.99414074420929. The probe's assertions passed, rc 0, no engine/script errors, editor SHA unchanged. This records the behavior boundary without changing production source; equal-bound sampling is not specified by the public contract. Evidence: `work/particle-gles3-scale-equal-zero-20261105/RECEIPT.md`, `result.json`, `run.log`.

The same six scale-over-velocity cases on fixed-editor Vulkan Mobile returned six finite active records per case using the RD 128-byte record and active flag at offset 76. Values matched Compatibility exactly: equal-bound below/at/above 1.0/1.0/2.99976491928101, zero/near-zero 1.0, moving control 1.99414074420929. Assertions and process passed rc 0. Evidence: `work/particle-mobile-scale-equal-zero-20261105/RECEIPT.md`, `result.json`, `run.log`.

The generated scale-over-velocity shader now guards equal reference bounds before division and explicitly chooses the step result observed by both renderer probes. The expanded extracted-source fixture is RED before the change (8/11, three zero divisions) and GREEN after it (11/11); the modified C++ translation unit passes syntax. Source-matched GPU replay is complete on both Compatibility and Vulkan Mobile; the fixed editor was restored afterward. Evidence: `work/particle-scale-equal-bounds-guard-20261105/RECEIPT.md` and the two renderer probe receipts.

The source-matched tests-enabled editor (SHA `6b8aa442e3843dabe4ef6904d7f03719481c438b07a092c2728de4772b53966b`) passed the six-case equal-bounds/zero-speed GPU probes on Compatibility and Vulkan Mobile, each rc 0 with empty failure arrays. All active scales were finite and retained the pre-change step and moving-control values. The fixed editor SHA `5feadf94...` was restored. This closes the bounded shader guard runtime gate on those renderers. Evidence: `work/particle-scale-equal-bounds-guard-20261105/{gles3,mobile}-source-matched.log` and its receipt.

The fixed editor rendered CPUParticles2D color-ramp stages on Compatibility and Vulkan Mobile: red/green/blue pixels `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256` for initial ramp, in-place edit, replacement, and mutation of the detached old ramp. Both runs exited rc 0 without engine/script errors; final PNG hashes matched and the fixed editor SHA stayed `5feadf94...`. Evidence: `work/particle-cpu2d-ramp-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes CPU2D visible ramp replacement for these renderers; CPU3D and product six-link remain separate.

The fixed editor rendered CPUParticles3D color-ramp stages on Compatibility and Vulkan Mobile using a white unshaded mesh with vertex color. Red/green/blue pixels were `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256` for initial ramp, in-place edit, replacement, and mutation of the detached old ramp. Both runs exited rc 0 without engine/script errors; final PNG hashes matched and fixed editor SHA stayed `5feadf94...`. Evidence: `work/particle-cpu3d-ramp-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes CPU3D visible ramp replacement on these renderers; product six-link remains separate.

The fixed editor rendered CPUParticles2D `PARAM_SCALE` Curve stages on Compatibility and Vulkan Mobile. Red-pixel area followed constant scale 0.5 → in-place 1.5 → replacement 1.0 → detached-old edit as `64 → 576 → 256 → 256`; both runs exited rc 0, final PNG hashes matched, and editor SHA remained `5feadf94...`. Evidence: `work/particle-cpu2d-scale-curve-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes CPU2D visible scale-Curve replacement on these renderers; CPU3D and product six-link remain separate.

The fixed editor rendered CPUParticles3D `PARAM_SCALE` Curve stages on Compatibility and Vulkan Mobile. Red-pixel area followed constant scale 0.5 → in-place 1.5 → replacement 1.0 → detached-old edit as `64 → 576 → 256 → 256`; both runs exited rc 0, final PNG hashes matched, and editor SHA remained `5feadf94...`. Evidence: `work/particle-cpu3d-scale-curve-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes CPU3D visible scale-Curve replacement on these renderers; product six-link remains separate.

The fixed editor rendered GPUParticles2D `GradientTexture1D` color-ramp stages on Compatibility and Vulkan Mobile. Red/green/blue pixels were `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256` for initial, in-place edit, replacement and detached-old mutation. Both runs exited rc 0 without engine/script errors; final PNG hashes matched and editor SHA remained `5feadf94...`. Evidence: `work/particle-gpu2d-ramp-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes visible GPU2D color-ramp replacement on these renderers; product six-link remains separate.

The fixed editor rendered GPUParticles3D `GradientTexture1D` color-ramp stages on Compatibility and Vulkan Mobile using a white unshaded QuadMesh. Red/green/blue pixels were `256/0/0 → 0/256/0 → 0/0/256 → 0/0/256` for initial, in-place edit, replacement and detached-old mutation. Both runs exited rc 0 without engine/script errors; final PNG hashes matched and editor SHA remained `5feadf94...`. Evidence: `work/particle-gpu3d-ramp-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes visible GPU3D color-ramp replacement on these renderers; product six-link remains separate.

The fixed editor rendered GPUParticles3D `PARAM_SCALE` CurveTexture stages on Compatibility and Vulkan Mobile. Red-pixel area followed constant scale 0.5 → in-place 1.5 → replacement 1.0 → detached-old edit as `64 → 576 → 256 → 256`; both runs exited rc 0, final PNG hashes matched, and editor SHA remained `5feadf94...`. Evidence: `work/particle-gpu3d-scale-curve-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes visible GPU3D scale-curve replacement on these renderers; product six-link remains separate.

The fixed editor rendered GPUParticles2D `PARAM_SCALE` CurveTexture stages on Compatibility and Vulkan Mobile. Red-pixel area followed constant scale 0.5 → in-place 1.5 → replacement 1.0 → detached-old edit as `64 → 576 → 256 → 256`; both runs exited rc 0, final PNG hashes matched, and editor SHA remained `5feadf94...`. Evidence: `work/particle-gpu2d-scale-curve-visible-20261105/RECEIPT.md`, `result.json`, raw logs and four PNGs. This closes visible GPU2D scale-curve replacement on these renderers; product six-link remains separate.

## 2026-11-05 — Compatibility heightfield X-extent live collision motion

A fixed-editor OpenGL 4.1 Compatibility probe changed a heightfield collision body's X extent from 2 to 8 while eight fixed-seed particles were already falling. The collision group stopped at Y `0.1099999`, while a same-seed collision-disabled control reached `-7.6466703`; a sphere positive control stopped at `1.00999999`. The outside-width negative control kept collision and disabled control equal at `-7.4288926`. All three checks passed, rc 0, active records were finite, and the fixed editor SHA stayed unchanged. Evidence: `work/particle-heightfield-extents-motion-20261105/RECEIPT.md`, `run-gles3-final.log`, and `result.json`. This closes a Compatibility live-X-extent motion boundary; Mobile parity and product six-link acceptance remain separate.


## 2026-11-05 — Compatibility heightfield X-extent shrink motion

A fixed-editor OpenGL 4.1 Compatibility probe started eight falling particles over a width-8 heightfield, then shrank the X extent to 2 during flight. The collision group first stopped at Y `0.1099999`; after shrink it fell to `-7.6466703`, matching the same-seed disabled control. Wide and shrink checks passed, rc 0, all records were finite with eight active particles, and the fixed editor SHA stayed unchanged. Evidence: `work/particle-heightfield-shrink-motion-20261105/RECEIPT.md`, `run-gles3.log`, and `result.json`. This closes the complementary live-X-extent shrink boundary on Compatibility; Mobile parity and product six-link acceptance remain separate.


## 2026-11-05 — GPUParticles2D scale Curve stop/resume synchronization

The same fixed-editor lifecycle probe passed on OpenGL 4.1 Compatibility and Vulkan Mobile: it drained GPUParticles2D, edited the retained scale Curve while stopped, resumed emission, then drained again and replaced the Curve before explicit restart. Red-pixel areas were `64 → 0 → 0 → 576 → 0 → 256 → 256` on both renderers; the replacement and detached-old captures had identical hashes within each renderer. All images were saved, both runs exited rc 0 without engine/script errors, and the fixed editor SHA stayed unchanged. Evidence: `work/particle-gpu2d-scale-curve-resume-20261105/RECEIPT.md`, raw logs, `result.json`, and PNG captures. This adds an interleaved stop/resume resource-sync boundary on both renderers; product six-link acceptance remains separate.


## 2026-11-05 — GPUParticles3D scale Curve stop/resume synchronization

A fixed-editor lifecycle probe passed on OpenGL 4.1 Compatibility and Vulkan Mobile while draining GPUParticles3D, editing a retained scale Curve while stopped, resuming, draining again, then replacing the Curve before explicit restart. Red-pixel areas were `64 → 0 → 0 → 576 → 0 → 256 → 256` on both renderers; detached-old mutation left the replacement frame hash unchanged. All captures were saved, both runs exited rc 0 without engine/script errors, and the fixed editor SHA stayed unchanged. Evidence: `work/particle-gpu3d-scale-curve-resume-20261105/RECEIPT.md`, raw logs, `result.json`, and PNG captures. This adds the corresponding 3D interleaved stop/resume resource-sync boundary; product six-link acceptance remains separate.


## 2026-11-05 — native World runner continuation diagnosis

A controlled native World GPU request log shows one deferred tick, continued EcsWorld E09 frames, no `NATIVE-CASE-RESULT`, and terminal close error 044. Read-only Godot source inspection shows `_enter_tree()` processing flags are registered and are not automatically cleared afterward. This narrows the unresolved gate to host process-group eligibility versus extension virtual dispatch; the terminal close error alone cannot establish early quiesce. The next discriminating replay must observe the host flags each frame and invoke one explicit process notification. Evidence: `work/particle-native-host-frame-audit-20261105/RECEIPT.md` and `result.json`; no production fix or successful runner claim follows. The broader Godot native queue remains active.


## 2026-11-05 — Godot rendering seal / native exit audit

An earlier audit asserted a retained World run with `particle_world_gpu_request` `3804/0` followed by a frame-seal error, guarded-preparation busy and close error 044, but its recorded raw log is no longer present and its parsed result flags are false. The currently available strict-close log is a distinct run: frame-seal error precedes `particle_world_gpu_request checks=61 failures=0`, with no observed busy/044 lines. Godot source still confirms `RenderingDevice::swap_buffers()` propagates `_end_frame()` failures across active lists, callbacks, preparation, transfer workers and latched frame errors. No production fix is claimed; a retained reproduction plus branch-level instrumentation is still required. Evidence: `work/particle-render-exit-seal-audit-20261105/RECEIPT.md` and `result.json`.

## 2026-11-05 — Vulkan Mobile heightfield live expansion baseline

The fixed editor ran a Vulkan Mobile variant of the Compatibility live X-extent probe with the verified RD 128-byte particle layout. Width 2 → 8 during flight left the collision and disabled controls equal at Y `-7.4288917`; the sphere positive control stopped at `1.00999999`. All groups had eight finite active records and 1024-byte readbacks, with no engine error. Version reconciliation shows this fixed editor predates the Mobile depth-clear change; the earlier static-width GREEN used source-matched editor `2eac37...`. This is a pre-fix binary baseline failure, not evidence of a distinct current-source live-expansion defect. No production source was changed. Evidence: `work/particle-heightfield-mobile-extents-motion-20261105/RECEIPT.md`, raw log, script and `result.json`; the next gate is source-matched live-expansion replay.

## 2026-11-05 — Forward+ live heightfield expansion and shrink

The same live-expansion script passed on Vulkan Forward+ with the fixed editor: 3/3 checks, rc 0, no engine/script errors, collision Y `0.10999990` versus disabled `-7.42889166` after width 2 → 8, and sphere control `1.00999999`. A complementary Forward+ live-shrink script passed 2/2: width 8 collision `0.10999990`, then width 2 during flight released it to `-7.42889166`, matching the disabled control. Evidence: `work/particle-heightfield-mobile-extents-motion-20261105/forward-plus-run.json`, `run-forward-plus.log`, and `work/particle-heightfield-forward-shrink-motion-20261105/RECEIPT.md`. No production source changed. Mobile current-source live mutation remains pending a source-matched replay.

## 2026-11-05 — Forward+ GPUParticles2D sub-emitter target replacement

The fixed editor's Vulkan Forward+ scene retained the GPUParticles2D source while removing and replacing its `../Target` at the same path. Green/blue pixels across baseline, old, removed and replacement stages were `0/0 → 560/0 → 0/0 → 0/576`. The runner passed 12/12 checks, rc 0, saved all four PNGs, reported no engine/script errors and preserved the fixed editor SHA. Evidence: `work/particle-sub-emitter-2d-replacement-forward-plus-20261105/RECEIPT.md`, `result.json`, raw log and PNGs. Exact sub-emission counts and product six-link remain separate.

## 2026-11-05 — Forward+ GPUParticles3D sub-emitter target replacement

The fixed editor's Vulkan Forward+ scene kept the GPUParticles3D source alive while its `../Target` was removed and replaced at the same path. Green/blue pixels across baseline, old, removed and replacement stages were `0/0 → 384/0 → 0/0 → 0/384`. The corrected runner passed 12/12 checks, rc 0, saved all four PNGs, reported no engine/script errors and preserved the fixed editor SHA. Evidence: `work/particle-sub-emitter-replacement-forward-plus-20261105/RECEIPT.md`, `result.json`, raw log and PNGs. Exact sub-emission counts and product six-link remain separate.

## 2026-11-04 — source-matched Mobile live heightfield expansion and shrink

After SCons treated the restored fixed editor as up-to-date, moving that verified binary aside forced a relink from current objects and produced editor SHA `6b8aa442...` with the Mobile depth-clear fix. The source-matched Vulkan Mobile expansion passed 3/3: width 2 collision/control both Y `-7.42889166`, width 2 → 8 midflight collision `0.10999990` versus control `-7.42889166`, sphere `1.00999999`. The complementary live shrink passed 2/2: width 8 collision `0.10999990`, then width 2 collision/control both `-7.42889166`. Both runs exited 0 without engine/script errors and had eight finite active particles per group. Evidence: `work/particle-heightfield-mobile-extents-motion-20261105/RECEIPT.md` and `source-matched-mobile-expansion.{json,log}`, plus `work/particle-heightfield-forward-shrink-motion-20261105/source-matched-mobile-shrink.{json,log}`. The source-matched editor was retained separately and the fixed editor restored to `5feadf94...`. This closes the Mobile live X-extent motion boundary; product six-link remains separate.

## 2026-11-04 — renderer seal branch trace admission gate

A bounded TESTS-only `_end_frame()` branch trace was built as a temporary Godot diagnostic and paired with the current ANGE native DSO for the World GPU request runner. The child exited 1 before rendering: E06 `loader.build_identity_stamp` expected 1 but observed 0 during `bind_main_device`. No seal branch ran and no root cause can be inferred. Exact editor/DSO hashes, command, raw output and rc are in `work/particle-render-exit-seal-audit-20261105/{trace-world.json,trace-world.log,trace-build.log}`. The temporary source instrumentation was removed and the fixed editor restored. A matching build identity/DSO pair is required before this diagnostic can proceed; the independent native queue remains active.

The fixed editor's controlled-build sidecar was then restored to its matching artifact hash, while the diagnostic editor and sidecar were retained as a separate matching pair. The precise hashes are appended to the seal receipt; A owns the matching DSO gate.

## 2026-11-04 — Vulkan Mobile vector-field sampled motion

A fixed-editor Vulkan Mobile GPUParticles3D probe retained one vector-field attractor and changed its 2×2×2 RGBA8 texture through +X, −X, clear and rebuild stages. Eight active finite particles per stage moved to mean X `2.03997 → -2.27997 → 0 → 2.27997`; each RD readback was 1024 bytes. The runner passed 11/11, rc 0, no engine/script errors, and fixed editor SHA remained `5feadf94...`. Evidence: `work/particle-vector-field-motion-20261105/RECEIPT.md`, `result.json` and raw log. Spatial interpolation, other renderers and product six-link remain separate.

## 2026-11-04 — Vulkan Mobile vector-field spatial sampling

The fixed editor ran two fixed-seed GPUParticles3D groups at world X −4/+4 inside one 2×2×2 RGBA8 vector field. Opposite X texel columns produced opposing displacements `+2.03994/−2.03994`; replacing the texture with reversed columns reversed the displacements to `−2.27995/+2.27995`. The corrected runner compares displacement from each group origin, passed 11/11, rc 0, with eight finite active records in each 1024-byte readback and no engine/script errors. Evidence: `work/particle-vector-field-spatial-20261105/RECEIPT.md`, `result.json`, raw log and probe. Exact interpolation weights, other renderers and product six-link remain separate.

## 2026-11-04 — Vulkan Mobile zero-scale 3D rotation velocity

The fixed-editor GPUParticles3D probe set `PARAM_SCALE=0` while applying Z rotation velocity, with a scale-1 control. Both returned eight active finite RD records. Zero-scale basis axes had length `0.00100000005` with a nonzero rotation off-axis term `1.97791e-05`; control axes had length 1 and off-axis `0.0197791`. The runner passed 13/13, rc 0, no engine/script errors and preserved editor SHA `5feadf94...`. Evidence: `work/particle-zero-scale-rotation-20261105/RECEIPT.md`, raw log and result JSON. This shows the ordinary Vulkan Mobile zero-scale plus rotation path stays finite; other renderers, rotation axes and product six-link remain separate.

## 2026-11-04 — zero-scale 3D rotation velocity on Forward+ and Compatibility

The same fixed-editor GPUParticles3D zero-scale plus Z rotation probe passed on Vulkan Forward+ and OpenGL 4.1 Compatibility, using their verified 128-byte and 64-byte particle records. Each renderer returned eight active finite particles, zero-scale basis axes length `0.00100000005` and nonzero rotation off-axis `1.9779107e-05`; scale-1 controls retained axis length 1 and off-axis `0.0197791`. Each process exited 0 with eight probe and six runner checks passed and no engine/script errors. Evidence: `work/particle-zero-scale-cross-renderer-20261105/RECEIPT.md`, per-renderer JSON and logs. Other rotation axes and product six-link remain separate.

## 2026-11-04 — GPUParticles2D backend collision radius synchronization

A new TESTS-only main-RD Vulkan Mobile control reads the same GPUParticles2D RID's production `collision_base_size` after texture 4×4, same texture resized to 8×4, replacement 2×2, detached-old mutation, and clear. Backend radii were `4 → 6 → 2 → 2 → 2` for base size 2. The source-matched editor SHA `b329c652...` passed the focused case 13/13 assertions, rc 0; fixed-editor neighboring `[GPUParticles]*` passed 25 cases / 138 assertions. Exact command, environment, binary/sidecar and log hashes are in `work/particle-gpu2d-collision-size-backend-20261105/result.json`, with raw build/run logs and receipt alongside. Source-matched editor and sidecar were retained separately; fixed editor and sidecar restored to `5feadf94...`/`57dbe31...`. This proves backend radius publication and detached-old isolation on one RD host; collision motion and other renderers remain separate.

## 2026-11-04 — GPUParticles2D canvas SDF motion after in-place texture resize

The fixed editor rendered real Vulkan Mobile GPUParticles2D with a LightOccluder2D canvas SDF. Same-seed red collision-enabled and blue collision-disabled systems matched at mean X `174.5` with no occluder. With the SDF present and 4×4 textures, red stopped at `155.5` while blue remained `174.5`; after both retained textures changed in place to 32×32, red stopped at `127.5` and blue again remained `174.5`. The runner's eight checks and probe's five checks all passed, rc 0, no engine/script errors; three PNGs, raw log, exact hashes and result JSON are in `work/particle-gpu2d-collision-motion-20261105/`. This is visible collision motion correlated with texture size on one Mobile host; the separate same-RID backend radius control supplies the storage value evidence. Other renderers and product six-link remain separate.

## 2026-11-04 — GPUParticles2D collision radius causal motion

A fixed-texture Vulkan Mobile Canvas SDF scene changed only the retained collision-enabled GPUParticles2D node's `collision_base_size` `2 → 16 → 2`, with same-seed disabled-collision and no-occluder controls. The red particle mean X changed `155.5 → 127.5 → 155.5`; blue stayed `174.5` throughout, and no-occluder red/blue both reached `174.5`. Every stage kept 16 pixels per group, excluding texture size as the cause; restored-small PNG hash matched initial-small. Runner 8/8 plus probe 7/7 checks passed, rc 0, no engine/script errors. Evidence: `work/particle-gpu2d-radius-causal-20261105/RECEIPT.md`, raw log, JSON and four PNGs. This closes visible radius-caused motion on Mobile; other renderers and product six-link remain separate.

## 2026-11-04 — current same-origin native World/direct seal gate

The fixed Godot editor and sidecar admitted the new native-test DSO SHA `3ed0e9f2...`. Visible Metal Forward Mobile World ran 96/0 native checks but logged `Failed to seal the rendering frame`; direct System ran 3/0 then crashed in Metal `presentDrawable` (runner exit -6) after two seal errors. Cancel ran 3/0 without observed seal error. These remain failing runtime gates despite assertion counts. The exact current logs and hashes are retained in `work/particle-render-exit-seal-audit-20261105/RECEIPT.md`. Godot owns `_end_frame()` branch diagnosis; A owns the single integration runner. Independent particle queue work continues.

## 2026-11-04 — GPUParticles3D one-shot restart timing

`GPUParticles3D::restart()` ended its emitting window early at high explosiveness because it assigned `emission_time = lifetime * (1 - explosiveness_ratio)`, unlike 3D `set_emitting(true)` and 2D `restart()`. A fixed 60 FPS SceneTree RED showed restart3D false at frames 30/45 while both controls stayed true. The one-line correction assigns `emission_time = lifetime`; isolated source-matched editor SHA `9eb089b7...` built rc 0 and GREEN ran rc 0 with all three true at frames 30/45, false at frame 90, and one `finished` signal each. Neighbor `[GPUParticles]*` tests passed 25 cases/138 assertions. Fixed editor/sidecar remain unchanged. Evidence: `work/particle-gpu3d-restart-timing-20261105/RECEIPT.md`, logs and rc files. This closes one-shot SceneTree timing, not visible GPU output or the product six-link.

## 2026-11-04 — seal diagnostic pairing and visible baseline

The retained diagnostic editor's exact CoreBuildSelection was extracted to `work/particle-render-exit-seal-audit-20261105/trace-core-selection.json` and accepted by ANGE's selection validator. This provides a matching DSO build input, but no diagnostic runtime branch result yet. A minimal Node2D Mobile scene on the fixed editor presented through Metal and exited 0 after 120 frames, with no seal error or crash. This narrows the visible failure beyond blank rendering; it does not identify whether the particle request, World content or teardown interaction is causal. Evidence and command are in the seal receipt. A currently owns the unique native-test builder, so the diagnostic pair is queued after its active build, while other Godot particle work remains active.

The same fixed editor also ran a visible Mobile GPUParticles2D scene with a live 64-particle emitter and ParticleProcessMaterial; the corrected probe printed ready/done, exited 0, and logged no seal or Metal present failure. Its exact command, raw log hash and probe hash are in the seal receipt. This excludes a trivial failure of every visible 2D GPU particle render, while the integrated World/direct path remains failing and requires same-pair `_end_frame()` branch evidence.

The matched diagnostic editor/DSO pair with `GODOT_RD_SEAL_TRACE=1` produced the missing branch evidence: `RD_SEAL reason=prepare error=44 frame=0 nonblocking=0 tickets=1 frame_issue=0 poisoned=0 draw=0 compute=0 ray=0 draining=0 callbacks=0`. This proves the World seal error came from Metal `qualification_prepare_segment()` returning `ERR_BUSY`, not the RD mutex/open-list/driver-callback preflight. Four Metal subpaths can return that code, including a first copy-queue commit that has a real native effect and retained observer; the exact subpath and retry rule remain to be established. Raw log SHA and pair identities are in the seal receipt. A's World fixture now rejects the seal diagnostic despite 94/0 or 103/0 native checks, so the false-green admission is closed while the runtime failure remains open.
