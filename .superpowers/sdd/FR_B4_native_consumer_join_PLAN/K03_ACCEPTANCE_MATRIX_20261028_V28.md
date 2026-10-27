# K03 acceptance matrix v28 — 2026-10-28

This successor matrix preserves v27 and adds the R01/E19/E22 cross-receipt source-identity and claim-boundary join. These packages do not change the fixed editor/DSO pair. Previous matrices remain historical evidence. The new ANGE commit is evidence-only: its diff contains no production source or build-graph paths, so the fixed editor/DSO pair remains unchanged.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| VFX projection consumer census | `1921759a6e` | `ff_vfx → WorldLoader → compile_system_bundle → EcsWorld validation`; `BridgeParticle` consumes ECS `Particle` only | projection → producer/operation owner |
| typed-close cancel owner boundary | `2008230c5f` | cancel binding/forwarder and fail-closed refusal | authenticated upload/record ticket and cancel disposition |
| typed-close parent closure | `82604358c4` | live obligations keep parent/service node attached; bounded Busy/requeue; detach only empty | typed producer child/operation receipts |
| external poll state | `e8f4781870` | exact generation; public `Created`/`Withdrawing`; withdrawal admission tombstone | owner completion/release observation |
| completion/readback join audit | `8b3b0af0ae` | QueueCallReport/NativeUse/fence return; BufferGetDataResult state delivery; renderer fail-closed guard | U08 admission + E09 same-request association before packet/readback publication |
| editor/DSO pair alignment | `409527e584` | exact current editor/DSO pair; non-headless Metal/mobile VFX/no-VFX source-stage, world entry, and explicit unload all rc0 | no external producer/operation/packet/readback owner |
| renderer/packet guard | `fc03fd5c18` | Forward+/Mobile external particle guard; NativeUse guard tests B4 20/419 and ParticlesTrail 1/5 rc0 | authenticated U08 producer and same-request E09 association |
| withdraw/retire lifecycle audit | `1d683b0c61` | withdrawal tombstone, Busy/Withdrawing, NativeUse retirement, authenticated Registry release and destructor order; focused B4 2/166 + 3/45 rc0 | public physical-disposal/Released receipt and owner producer association |
| final pair VFX/U01/close recheck | `6ead13473f` | final1 editor/DSO pair; VFX/no-VFX source-stage and world entry/unload rc0; explicit `exit_world()` active true→false and unload rc0 | superseded by final2 pair for current provenance |
| final2 pair VFX/U01/close recheck | `1b08fb7519` | final2 editor/DSO pair; VFX/no-VFX source-stage and world entry/unload rc0; explicit `exit_world()` active true→false and unload rc0 | U08/E09 operation/packet/completion/readback/physical/Ready/GPU evidence |
| D VFX projection validation/rollback audit | `290383df79` | source-only fail-closed projection publication, pre-mount validation/rollback, and same-path runtime erase guard | no binary attribution; no packet/operation/GPU authority |
| E03/E04 current-source owner boundary | `65b8469e1b` | source/CPU consumer evidence E03 17/1187 and E04 3/65; exact production UNAVAILABLE owner fields | accepted original preparation/source/session/detach authority and R01/K01 issuer |
| E34/P01 current owner boundary | `5bd8124839` | adapter no-root/drift fail-closed, selector 2, parent contract 4; clean bootstrap unavailable | authenticated parent root, OwnerPayloadBridge/E35 ABI, accepted bootstrap executor |
| I02 imported-texture consumer | `e645c1ca92` | CPU material/lint consumer exact 1/30 and family 19/7671 | no P01/E34/native/GPU/Ready authority |
| current-A consumer boundary set | `e1ed218af9` | E05/E17/E21/E22/E23/E30 source/CPU consumer slices with per-snapshot rc0 evidence | E20/owner/native/GPU/Ready authority remains absent |
| current-A/E34 successor boundary set v2 | `b092756b25` | E34 bootstrap successor plus E21/E22/E23/E24/E25/E26/E28/E29/P4/scene-install source/CPU/contract slices | parent/owner/native/GPU/Ready authority remains absent |
| external owner handoff blocker map | `cad566bb89` | exact E03/E04, E05/E20, E34/P01, and U08/E09 owner inputs with fail-closed local behavior | owner declarations/bridges remain external inputs |
| E09/R01 current CPU consumer | `91cc0084cb` | R01 dispatch 14/2051 plus extent/handoff/origin/upload-data CPU assertions | no RenderingDevice/native submission/readback/GPU/Ready authority |
| godot_native_queue_landed | `4968f2ce23` | GDScript lifecycle/reload, RD external mesh/packet/camera guards, Curve bake-cache fix; focused tests/build rc0 | external U08/E09 owner and qualification inputs |
| godot_native_queue_pair_recheck | `f9bd40569c` | fresh editor/DSO pair; VFX/no-VFX/explicit close non-headless Metal/mobile rc0 | U08/E09 operation/completion/readback/physical/Ready/GPU evidence |
| d_vfx_diagnostic_rebuild | `253d6fa22` | DSO rebuild rc0; malformed source FILE-001 diagnostics, rollback, unload | external semantic issuer/owner; no qualification |
| godot_native_queue_pair_recheck2 | `dbf12c241b` | post-D-rebuild editor/DSO pair VFX/no-VFX/close rc0 | U08/E09 operation/completion/readback/physical/Ready/GPU evidence |
| ange_native_cpu_focused | `b3f621b1bd` | ANGE bundle/E05/E06/C02 CPU/native-guard 69 cases / 1640 assertions rc0 | no U08/E09 producer, operation/completion/readback/Ready/GPU authority |
| d_vfx_projection_guard_rebuild | `c2e966c6c7` | D projection validation before mount, same-path erase guard, fresh DSO and four runtime probes | external semantic producer/owner; no operation/completion/readback/Ready/GPU authority |
| godot_native_producer_caller_gap | `dffea6e447` | source census: zero production external-surface producer callers; Forward+/Mobile consumer guards preserved | authenticated U08/E09 producer, same-request association and operation owner |
| ange_lifecycle_sprite_caller_gap | `cde560a0ed` | source census: no lifecycle symbols or lifecycle→attribute→Sprite/GPUParticles3D production caller | lifecycle producer/attribute/Sprite owner and native/GPU completion |
| ange_e09_production_caller_gap | `35f0a58de5` | bind_main_device real callsite; zero production request/poll/acquire/qualification callers; first missing producer/admission boundary | original producer, same-request E09 association, operation/completion/readback owner |
| ange_native_reader_diagnostic | `7bce63f51b` | E05 native readers 1/12 and E06 diagnostics 1/13 rc0 against current DSO | no producer/operation/completion/readback/Ready/GPU authority |
| ange_e05_typed_close_focused | `bbe55abcf0` | ANGE E05 typed-close/native reservation focused suite: 11 cases / 210 assertions rc0; exact source/test binary/DSO/log hashes recorded | CPU/in-process only; no U08/E09 producer, operation/completion/readback/Ready/Verified/GPU authority |

| ange_semantic_vfx_focused | `b78c7b4ce3` | ANGE semantic/VFX focused suite: 12 cases / 141 assertions rc0 across registry qualification, system bundle, parser/loader, and stage admission | CPU/in-process only; no U08/E09 producer, operation/completion/readback/Ready/Verified/GPU authority |

| ange_e09_caller_gap_recheck | `fdf4ae16b7` | Current-source census distinguishes internal `ProgramPreparationStore::acquire_native()` from absent public loader/StageExecutor production callers; bind_main_device has one registration path | original particle producer/admission and same-request E09 association remain external owner inputs |

| ange_current_host_particle_focused | `1ae829042d` | Current ANGE host/particle focused tags: 54 cases / 1,772 assertions rc0 with explicit isolated-child executable environment | CPU/in-process only; no U08/E09 producer, operation/completion/readback/Ready/Verified/GPU authority |

| ange_current_owner_caller_census | `b5ea6a2740` | Current ANGE/Godot source census after host/capture commits; internal acquire distinguished from absent public producer/qualification callers; mesh producer remains zero | original particle producer/admission and same-request E09 association remain external owner inputs |

| ange_current_dso_pair_rebuild | `585a4778b7` | Current signed DSO/editor pair: VFX/no-VFX/explicit-close rc0; malformed VFX rc1 with rollback/unload; exact editor/DSO hashes recorded | no U08/E09 producer association, operation/completion/readback/Ready/Verified/GPU authority |

| ange_current_provider_oracle_focused | `bb43cef8e1` | Curve/mesh/texture pure provider oracle suites and mutation checks green; texture runner maintenance mismatch recorded separately | pure CPU semantics only; no owner/capture/native qualification/completion/readback/Ready/GPU authority |

| godot_real_particle_caller_boundary | `ca6ad052cd` | Real `BridgeParticle` production consumer at `sys_visual_bridge.cpp:328` creates `GPUParticles3D`; public native qualification and external mesh producer callers remain absent | ParticleData→U08/E09 association and native producer owner remain external inputs |

| ange_texture_provider_runner_refresh | `68b94bb466` | Texture provider runner corrected to current 5,242 assertions and new 12-assertion census section; baseline/restored/mutants green | private harness maintenance only; no producer/owner/native qualification/completion/readback/Ready/GPU authority |

| ange_e05_e06_storage_consumers | `65252b259` | Current ANGE `test-host-storage` CPU/in-process consumer slice: 22 cases / 681 assertions; required child executable supplied for the green library-generation run; no source/build-graph change | configuration red is retained separately; no external owner, RenderingDevice/native/GPU/Ready qualification |

## Current fixed pair

- Godot editor SHA: `bbe896df5fa85f96af16a7ca94d7737641a861fb082e8b3d47606d7a2fef3be8`
- Embedded engine version hash: `c2bb425e81c0c6ea69e74fcf23d45fa40c7b7f01` (build stamp).
- Godot source HEAD at build: `c2ebe2578021378700e0db0cd707d7406f6eecc6`.
- ANGE source HEAD: `2aca8b14aa93a0f0c1707e62887b49ca2109e685`.
- ANGE DSO SHA: `b672d4148aba2cbd40c847d8125732d9fa84b2f217bfeb05a0802f9143cb492d`.
- VFX: entries 2 / systems 1; extension/world/unload true.
- No-VFX: no VFX stage; extension/world/unload true.
- Explicit close: active true→false; extension/world/unload true.
- Malformed VFX: rc1; VFX-SYN-001; rollback and unload true.

## Claims

U08/E09 association, operation, completion, readback, packet publication, physical disposal, Released, Ready, Verified, and GPU qualification remain false. D diagnostic classification and local pair success do not manufacture external owner authority.

| ange_c04_spirv_test_graph_blocker | `b92836037` | Two SPIR-V Catch2 sources compile in isolation but are absent from SConstruct/default `ange_test`; standalone gpu_linking is separately bounded; link probe records missing reflection instrumentation symbols | exact test-graph/instrumentation blocker; no source/build-graph change, default coverage, native/GPU/Ready qualification |

| ange_provider_dependency_closure | `0774e6c7d` | Pure provider source/test closure with strict Debug/Release/UBSan matrix, oracle replay, and C05 shape helper follow-up (texture 5,242 assertions) | CPU provider semantics only; no E20 ownership, host admission, native capture, GPU qualification or Ready |
| ange_provider_math_successor | `4a7bd5019` | Provider ribbon 11/211, connected ribbon 2/91, surface math 1,340, spline math 47/2,965; all rc0 | pure CPU consumers only; no native/GPU/owner/Ready authority |
| ange_writable_ribbon_successor | `c795db09d` | Writable resource 25,150 assertions and ribbon geometry 7,742 assertions; both rc0 | pure CPU consumers only; no native/GPU/owner/Ready authority |
| ange_spirv_destroy_diagnostic | `cc4e11518` | `test-spirv-destroy` builds rc0 but current run aborts rc133 in mfm_free→ange_real_destroy; untracked fixture retained as diagnostic | no destruction pass, native/GPU, owner or Ready claim; production fix remains open |
| ange_track_off_link_diagnostic | `1b74d1977` | Track-off object universe compiles, final link rc2 with 63 missing Godot C++ SDK symbols; no binary run | linker/SDK closure blocker only; no track-off, native/GPU, owner or Ready claim |
| ange_scene_install_v2_source_pin | `ef3f611bb` | V2 materializer/callback and SDK observer source pin: 8 exact paths, 78 inherited V1 rows, 18 V2 contracts, 7 mutants | D producer, native/GPU, P4 and Ready remain absent |

| ange_c01_identity_successor | `e9b9de3a8` | Fresh default binary C01 identity consumer: 3 cases / 74 assertions rc0 | passive identity fields only; no owner/runtime/native/GPU/Ready authority |
| ange_c05_descriptor_harness_successor | `ef334ca2a` | Fresh default descriptor/capture/guard/allocation/physical-projection consumer: 10 cases / 2,511 assertions rc0 | CPU descriptor consumer only; no native/GPU/owner/Ready authority |
| ange_c04_c07_source_graph_blocker | `7306f2135` | Read-only mapping of prior rc2 linker symbols to omitted method/capture provider TUs; no SConstruct/source edit | source-graph closure blocker; no standalone pass/native/GPU/owner/Ready claim |
| ange_c05_image_reader_path_blocker | `00ec830b7` | Private image-reader evaluator exits rc1 before compile because its ROOT calculation points under `tests/particle/src/...`; no runner/source edit | evaluator path blocker only; no reader/native/GPU/owner/Ready claim |
| ange_default_test_graph_mismatch | `390d9c96c` | Fresh default binary R01/E06 six-filter probe: all no-match rc2; expected literals absent from binary | default coverage/source-graph blocker; no R01/E06/full/native/GPU/owner/Ready claim |

| ange_c04_spirv_test_graph_refined | `85e38ba47` | Private minimal link now rc0 and runs 54 Catch2 cases (53 passed / 1 allocator-contract red); default SConstruct/tag coverage remains absent | owner-approved source/test decision remains open; no default/native/GPU/Ready claim |
| ange_default_freshness_c01_c05_successor | `bd51f0c00` | `scons -Q test` refreshes 3 stale paths rc0; fresh default binary C01 6/129 and C05 policy/descriptor 9/1,274 rc0 | focused CPU consumer/freshness only; no full suite/native/GPU/owner/Ready authority |

| ange_c02_layout_successor | `fd555ffad` | Fresh default C02 descriptor/address/storage-binding consumer: 17 cases / 212 assertions rc0 | CPU layout/addressing/storage-binding only; no native/GPU/owner/Ready authority |
| ange_c05_template_content_successor | `f48d74de6` | Fresh default InternalTemplate/qualification-content consumer: 3 cases / 2,179 assertions rc0 | CPU projection/readport/lifetime only; no native/GPU/owner/Ready authority |
| ange_dimensions_allocation_successor | `d321f2a2a` | Allocation target passes 9,192 cold failure sites; diagnostic target passes SEM013 (9) and SEM015 (10) recovery cases | CPU/OOM dimensions diagnostics only; no native/GPU/owner/runtime/Ready authority |
| ange_e30_e33_channel_current | `287fdecf2` | Debug/Release/UBSan channel variants each pass 8 cases / 93 assertions | pure channel arithmetic/ordering/spawn/statistics; U03/U04/E07/E11/R01/E09/native/GPU/Ready remain open |

| ange_c04_allocator_source_test_mismatch | `6300dae81` | C04 private link runs 54 cases with 53 pass / 1 allocator-contract mismatch; source/patch/counter hashes pin the mismatch | source/test contract decision remains open; no fallback, rewrite, default/native/GPU/Ready claim |
| ange_c07_artifact_cache_successor | `da4f6fe6b` | Artifact-cache target rc0; 11 cases / 316 assertions rc0 | CPU cache behavior only; no native/GPU/owner/runtime/Ready authority |
| ange_c05_method_association_successor | `97a398540` | Association runner rc0: 34 retained OOM points, 110 source association points, 87 exhaustive mapping points, canonical admission/mapping pass | CPU/OOM consumer only; no native/GPU/external owner/U08/Ready authority |
| ange_c05_main_rd_successor | `f86f0ab8c` | MainRD storage target rc0; 14 cases / 1,635 assertions rc0 | CPU transport only; no owner installation/native/GPU/Ready authority |
| ange_c04_c07_link_diagnostic | `62c0dba19` | Grouped standalone targets link rc2 with missing VFX/semantic helper symbols; no binary run | source/build graph diagnosis only; no C04/C07 pass/native/GPU/owner/Ready claim |

| ange_gpu_policy_successor | `d2aa27e43` | Seven standalone policy targets build/run rc0: fixed provider, qualification static/unit, IR allocation, linking, numeric allocation, method qualification | source/IR/policy only; no live RenderingDevice/GPU owner/Ready qualification |
| ange_mesh_provider_successor | `e8b297ec8` | E25 mesh source 555, E26 exact 4,027, sampling 506 assertions; all rc0 | pure CPU mesh provider only; no GPU/native/owner/Ready authority |
| ange_e22_e23_provider_successor | `7aeeeb976` | Curve 2,108, host snapshot 1,953, resource snapshot staging rc0 | pure/controlled CPU E22/E23 only; no E20 owner/D/core/native/GPU/Ready |
| ange_e22_texture_provider_successor | `d1b7a6cac` | Texture provider 5,242 assertions, 90 policy rows, 670 oracle vectors rc0 | pure CPU texture policy/vector closure; no owner/D/core/native/GPU/Ready |
| ange_e09_r01_successor | `f47dd0d63` | R01 14/2,051; extent 109; handoff 57; origin 44; upload data 781; all rc0 | bounded CPU/in-process only; no RenderingDevice submission/readback, accepted E09/E06 owner, GPU/PCK/Ready |
| ange_e05_caller_boundary_successor | `5e46fc4c9` | Host-storage focused 91/7,801 plus boundary 9/358; production caller chain reaches E05 `host.storage_owner_transport` UNAVAILABLE | authentic E05 owner input remains open; no synthetic owner/fallback/native/GPU/Ready |
| ange_e34_p01_boundary_successor | `18d44e11b` | Selector 2/2, parent contract 4/4, clean bootstrap 1/1; adapter without acceptance root rc3; forged settings refused | no parent adoption/owner bridge/native/GPU/Ready; acceptance root remains external |
| ange_k03_dso_rebuild_successor | `18d44e11b` | Shared dirty Debug DSO postlink product codesign verify rc0; terminal marker inferred, not clean-commit proof | no Release/UBSan/full-test/native/GPU/owner/Ready claim |

| ange_c04_spirv_opt_in_target | `80008a266` | SConstruct opt-in `test-c04-spirv` builds rc0 and runs 66 cases / 6,974 assertions: 64 passed, 2 contract red | default graph remains unchanged; allocator/reflection contract failures remain explicit; no native/GPU/Ready claim |

| ange_r01_e19_e22_consumer_join | `268d25e6f` | Five predecessor receipts joined at current HEAD: 42 unique source paths, 40 unchanged, 2 drifted/missing; no new runtime run or source/build-graph change | join/claim-boundary evidence only; R02/E19 owner, native/GPU and Ready remain open |
