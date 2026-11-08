# Native preparation continuation execution ledger

Plan: /Users/macbookair/Documents/Work/Dev/ANGEParticleDesign/docs/contracts/b4-native-output/GODOT_NATIVE_PREPARATION_CONTINUATION_PLAN.md

Base: adbbfb7be6. Main coordination explicitly delegated full Core/renderer timing and implementation to D; B only supplies its original selected final source/preparation inputs. First-phase host stays fixed. Default Sol only.

## Current task

Task 1 in progress, not accepted. Added foundational private ProductionOutputPreparationScope and pipeline factory overloads in Core/RD; original physical API gates remain unchanged. Tests-first opted read-capacity fixture expects Acquire to reserve closed pipeline custody before the RECORD frame expires. Production Acquire reservation behavior has not changed yet, so its behavioral RED is pending.

RED build: SCons session 4366, process 52788, log /tmp/b4-native-continuation-red-build.log. RD/header changes cause broad dependency recompilation. Do not start a duplicate build. Poll that exact session/process; do not treat observation timeout as termination.

Command: `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`.
Then run: `bin/godot.macos.editor.arm64 --headless --test --test-case='*[NativePreparationContinuation]*'`.

## Shared edit ownership

- prefix_recovery_implementation: Core request/guard/output, RD header/immutable_hold, later owned maintenance and rendering_device_output_retire guards. Frozen until RED finishes. Await root permission for Acquire reservation insertion.
- root: ParticlesStorage helpers, owner row/inflight guards, tests/core/extension/test_production_request.cpp and custody bridge/owner-state tests.
- generation_surface_borrow_sol: Mobile-only shared native surface selector extraction and focused tests. Patch prepared in /tmp/mobile-selector.h, /tmp/mobile-selector.cpp, /tmp/mobile-selector-test.cpp; workspace untouched until root permits after RED.
- native_continuation_plan_review_sol: read-only plan/Core/RD review. Original plan corrections incorporated; final operative NewWork clause corrected after scoped re-review.

## Required next work

1. Finish RED build/run; expect original Acquire reservation checks to fail without native behavior changes.
2. Let Core implement dry spare check and reserve original pipeline groups while RESERVING, only opted P>0/G>0 and actual nonzero native resource layout. Root adds `_external_pipeline_reserve_owned_prefix` keeping row closed and no cached physical context; original helper remains unchanged for old tests.
3. Add RT callback test for native scope after actual record consume: exact row/group/root, old-context refusal, no BOUND replay, guarded unpin/OBSERVE/service, actual owned pipeline factory and bounded retirement. Add owner row/service guards plus source_sealed admission check.
4. Build and run focused/native prefix/alias/owner neighbors; independent review and explicit-file commit. No Task1 qualification until runtime results exist.
5. Apply Mobile shared-selector extraction, verify ordinary behavior and exact tuple tests; continue full six-task plan, including bank/capture/read/draw/no-follow-up-draw closure.

## Boundary corrections already fixed in plan

Preparation groups P*(2G+4) exclude borrower headroom L, which is paid separately from original resource_uses. G means exact transitive capture closure including reverse edges, embedded sampler/texture parents and prospective VA/pipeline edges. Generation-retained source geometry uses existing live-snapshot transfer, not detached-directory custody. NewWork admits new scope/create; exact retained-scope CloseOldWork permits mandatory capture/adopt/cleanup seal after cancel. Inflight must block mutation/service/reuse as well as unpin/destruction, with Registry -> RD lock order.

B corrected its initial compatibility report: current DTO transport can carry P/G but real native-output producer and total preparation budget are not yet connected; compute-binding resource-use checks do not prove this budget. B continues its own actual action/source producer. D must not claim it exists or fabricate its inputs.

No visible Sprite, public Instance192 gate, GPU/FullBirth or whole-goal completion is claimed.

## Resume checkpoint 2026-11-08

Long goal remains active; user requests fewer Astra calls. Default Sol used for all current work.
RED session 4366 completed exit 0 (16m23s). Exact NativePreparationContinuation run failed as expected: 1 case, 779 assertions, 3 failures, all original Acquire missing pipeline reservations; log /tmp/b4-native-continuation-red-test.log.
Core agent now authorized to insert dry spare and original RESERVING reservation. Mobile selector extraction applied and diff-check clean; no runtime result yet. Owner inflight mutation guards applied by Sol; root added service guard, source_sealed admission, closed reservation helper and original post-RECORD native scope callback tests. Final coherent build still pending; no GPU/visible draw claim.

## Task 1 accepted checkpoint

Final coherent build exit 0. NativePreparationContinuation 3/2926 pass; alias/Mobile/source neighbors 9/2932 pass; owned pipeline/storage/layout/public-metadata neighbors 17/12762 pass. Scoped independent Sol review cleared both findings. See NATIVE_CONTINUATION_TASK1_RECEIPT.md. Final SHA 3cbc9e04a659128008bd22d4ea63e814212c87fda48c6f987b404b4d89293aba. Private scope foundation only; complete bank/capture/read/public caller and real GPU acceptance remain open.

Main additionally delegated the complete caller successor: pre-reserve bounded RT MetadataQuery (original MainRDStorageTransport), public query/CreateV3 actual nonzero limits+generation selection/slab custody, original U05 registration/native camera/host/phase association and late completion facts, E09 record-token Seal, real renderer/read/draw/close. Unique interface boundary main: ANGEParticleDesign/docs/contracts/b4-native-output/GODOT_NATIVE_CALLER_INPUT_PACKAGE.md. Prefix agent prepares copied-payload typed RT route candidate under work/native-output-prequote-rt-20261108/candidate; it never edits fixed P1/A/C live tree.

## Task 2 metadata slice checkpoint

Explicit preparation_count decoupled from binding_count; generation quotes global read capacity, separate from per-action P. Behavioral RED2case430assert4fail; GREEN17case6987assert pass, buildexit0, scoped Sol review clear. See PREPARATION_BANK_LAYOUT_RECEIPT.md. Atomic P-row bank/tuple/scope/capture/retire remains next. Core Acquire slab recomputation explicitly carries preparation_capacity.

Prequote original RT candidate syntax checks passed but runtime integration is pending; independent review and existing adapter/Core fixture tests are being prepared. Current candidate is not public Instance192 or U05 API acceptance. Main corrected canonical numbering: native request canonical is issued in actual nondecreasing group order, original shader canonical/source/substep remains in frozen typed sidecar; no Acquire(S)→shader0 fallback. Three native control stages Acquire/FinalizeGuardCopy(with JD02 token)/Seal remain required, not yet exported.
