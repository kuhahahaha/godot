> Original implementation-agent handoff. Later parent execution is recorded in [RECEIPT.md](RECEIPT.md), [CPU_RESULT.json](CPU_RESULT.json), and [FINAL_BUILD_SOURCE.json](FINAL_BUILD_SOURCE.json). [RAW_OUTPUT_MANIFEST.json](RAW_OUTPUT_MANIFEST.json) indexes the original raw evidence.

# Sprite preparation metadata slice — frozen for parent build

Base HEAD: acd6310a0b84ff80ea90cec106f4a0e9ec7a855c. Exact eight changed source/test byte hashes are in SOURCE_FREEZE.json. No build, CPU test or GPU run was performed by this agent. No commit/stage/push. This is an implementation candidate, not runtime qualification or public Sprite admission.

## Actual production changes

- The original metadata planner charges one stable ExternalPipelinePreparation row per already quoted SurfaceBinding. Alignment/checked u32 extent rules remain shared by quote, allocation, validation and acquire. No independent allocator or public ABI field is added.
- Original metadata installation placement-constructs the finite row directory. Real Registry::production_output_acquire associates its existing RESERVING action's slot with those exact rows before storage allocation; each row carries that owner/generation/lease incarnation. All rows remain closed (source_sealed=true, max_resources=0), with no selected native pass or pipeline authority.
- bind_preparation_rows preflights the complete finite slice before any association; duplicate, foreign occupied and out-of-range rows cannot be overwritten. Original action/root/slot/lease admission is unchanged in the caller. No arbitrary BOUND-pin factory admission is introduced.
- No-native-prefix acquire rollback returns its unopened rows. A real storage prefix retains the row with the original slot. Original bounded slot OBSERVE resets the row before dropping SurfaceBinding. Generation directory return refuses unresolved row state and destroys its placement objects before metadata release.
- The existing pipeline prefix entry now requires the exact assigned preparation row/address and slot lease. Its old factory/owner authentication remains unchanged. It still has no production caller or opener.
- Test-only ExternalLinkedOwnerTest friendship in external_output_packet.h permits inspection; it adds no production state or issuer.

## Tests and exact scope

The tests-first patch is tests-before-production.patch (e9bb9a15dc15a15064f751c6a4fe25cce0fa976b0766d9736951fad5fd4a5ae6). It requires new fields, so against old source it would be a compile failure, not an observed behavioral RED. No red/green claim.

Existing real metadata/create fixtures now inspect the preparation slab and final directory emptiness. The combined SurfaceBinding+preparation layout tests check alignment/disjoint extents, largest accepted/first overflowing count and unchanged output on refusal.

Two checks in NativeAliasReadbackProbe/output_alias_readback_complete inspect the actual internal acquired slot, not a synthetic pin. The first also calls the metadata binder with the already bound slot (ERR_ALREADY_IN_USE) and out-of-range slot (ERR_INVALID_PARAMETER), then validates original row identities. The second follows the actual request-owner OBSERVE drain and verifies those rows were reset before generation retirement. Existing integration/map-failure/cleanup-content variants all use these checks.

Parent selectors:
- *[GenerationMetadataLayout]*
- *[GenerationSelectionLayout]*
- *[GenerationSelection]*
- *[OutputSceneMetadata]*
- *[AliasReadbackIntegration]*
- *[AliasReadbackMapFailure]*
- *[NativeSceneReleaseMaintenanceBudget]*

No second authentic output request/action is added, so actual slot reuse is not newly covered. No fake incarnation reset, record token, native append, pass grant or completion is introduced. Existing short metadata/refund branches cover pre-allocation refusal; no new storage-factory failure harness is claimed.

## Remaining boundary

The original acquire input contains pass_mask but no exact framebuffer/subpass/shader-version/specialization selection. There is no authentic native pass opener to drive external_pipeline_prepare. This slice therefore does not prepare pipelines, seal packets, issue read leases, record visible draws or sign no-next-draw last-use closure.

The old _external_pipeline_prepare_owned_prefix still calls the pre-action owner_bind; owned factories only additionally allow the exact current NATIVE_OUTPUT RESERVING action. A future real renderer preparation must occur in that original opportunity or add a reviewed narrow original-action continuation. It must not open arbitrary BOUND pins or replay an expired physical context.

Important failure obligation for that future opener: if it changes source_sealed=false/max_resources>0 and owner_bind or owned_reserve then refuses without a group, surface.preparation is not yet installed. It must restore the exact no-effect row to closed state or retain a serviceable authentic prefix. Current source has no opener, so this branch is not production reachable or tested here. This receipt does not claim opened/no-group failure cleanup is complete. Native pipeline/material/geometry prefix transfer remains the next slice.

Metadata byte requirements increase; callers must obtain native requirements again. Public ABI, production_window, public Instance192 Create and all seal/view/resolve UNAVAILABLE gates remain unchanged.

## Static checks and separate CI repair

clang-format 21.1.7 --dry-run --Werror passed on all eight files; git diff --check passed. No compilation or execution was permitted to this agent; parent owns both.

android-contention-initializers.patch is separate from preparation-rows-only.patch. It changes three existing RequestTU Contention structs: std::mutex mutex{}, std::condition_variable condition{}, and two Semaphore ready{} declarations. These fix observed Android -Wmissing-field-initializers without suppressing diagnostics. It also renames the two ManagedSceneHarness local device variables to actual_device, including their only local call references, to fix observed MSVC C4458/C2220. SHA256 7ef0f2b659d404e20fc1ca6967edc352639ee2990a700713edf5322d881141eb. The row-only patch excludes all initializer and local-name edits; frozen full RequestTU bytes include both sets.
