# K03 acceptance matrix v10 — 2026-10-27

This successor matrix preserves v9 and adds the focused ANGE E05 typed-close native regression package. Previous matrices remain historical evidence.

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

## Current fixed pair

- Godot editor SHA: `d4938be792ae05f6868f73d13270c8ebe92292043eadadb6114717752924bb9f`
- Embedded engine version hash: `c2bb425e81c0c6ea69e74fcf23d45fa40c7b7f01` (build stamp).
- Godot source HEAD at build: `c2ebe2578021378700e0db0cd707d7406f6eecc6`.
- ANGE source HEAD: `bba55f3ba`.
- ANGE DSO SHA: `38c3da7e20377ed7988c683ffd7e98db0e1ea10fb3979916d321466bfc260252`.
- VFX: entries 2 / systems 1; extension/world/unload true.
- No-VFX: entries 1 / systems 0; extension/world/unload true.
- Explicit close: active true→false; extension/world/unload true.

## Claims

U08/E09 association, operation, completion, readback, packet publication, physical disposal, Released, Ready, Verified, and GPU qualification remain false. D diagnostic classification and local pair success do not manufacture external owner authority.
