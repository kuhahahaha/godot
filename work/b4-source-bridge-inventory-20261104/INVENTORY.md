# B4 source bridge inventory

Read-only snapshot; this is an implementation inventory, not a replacement contract or new plan. No build/test/source mutation or cross-chat message was performed. Main ANGE and the active phase1 integration tree contain other-owner dirt; the exact read bytes and searches are in SOURCE_SNAPSHOT.json. This report does not accept those dirty changes.

## Existing reusable owners

| Source | Actual interface / boundary |
| --- | --- |
| ANGE main `src/engine/particles/attribute_storage.h:164–170` | `StorageOwner::reserve` and `borrow_part` retain an original storage allocation/part. This is allocation lifetime, not an E05 whole published root or final-selection proof. |
| ANGE main `src/godot/particle/main_rd_storage_transport.cpp:366–376` | `MainRDStorageTransport::read_backing(const StoragePartBorrow&)` requires original session, live carrier, slot/part, allocation generation and exact byte size, then projects the D-issued backing. Keep the original pin through RT use. A copied RID alone is insufficient. Latest shared storage change: e00103fe2beb9cb901996ec0c585b340ddc11144. |
| ANGE main `src/godot/particle/particle_program_loader.h:71–74` and `native_unit_port.h:47–48` | Original loader/native hold projects the native shader/pipeline backing and main device/session; no arbitrary raw RID enrollment. |
| Godot `servers/rendering/rendering_device.h:400–436` | `NativeAppendEffect`, `CopyAppendResult`, `DrawAppendResult`, `_buffer_copy_recorded`, `_draw_list_draw_indirect_recorded` expose exact append/prefix/Unknown mechanics privately. They do not grant original source ownership or aggregate list adoption. |
| Godot `servers/rendering/rendering_device.h:1423–1427` / `.cpp:8768,8792,8838,8901` | New private Core storage/set create, exact physical-return observe/free/maintenance. Friend is `RendererRD::ExternalOutputStorage`. That class has no production definition yet. Commit 20f6476ecb4baaf25c515a8e48bda884fe7b8a5f is mechanical disposal, not an E09 source bridge or overall OutputReleased. |

## Narrow System route in the active integration tree

`/Users/macbookair/Documents/Work/ANGE_phase1_integration`, HEAD debcccb436726ec88d2380284f434e312f6c4d2c plus owner dirty:

- `src/godot/particle/e09_native_recorder.h:118–123` and `.cpp:318–359`: `start_system` accepts an original `QualifiedCheckedSystemBindingSetLease` and schedule ordinal. It requires scalar dispatch `{1,1,1}`, one invocation, 1–4096 output words, and bounded bytes. This is a scalar System controller, not an Instance192/Ribbon export controller.
- `src/particle/stage_execution/record_session.h:31–34,138–140,172–175` and `.cpp:350–404`: `SystemRecordProvenance` retains the original retirement generation/range plus request nonce; reserve/require/inspect join the original checked binding and schedule ordinal.
- `src/godot/particle/e09_native_recorder.cpp:818–826` obtains that original provenance and creates the fixed System scalar request using the retained native unit/output backing/allocation generation/session. `OriginalDeviceSessionRef::system_scalar_record_owner` (`device_session_adapter.cpp:291`) invokes the real core System scalar port.
- Godot `core/extension/gdextension_system_scalar.cpp:48–105` validates that fixed scalar layout, bridge-owned unit/storage exact identities and allocation generation, one writable buffer, no push constants and local group size 1. It allocates a System scalar ticket. That ticket is not a B4 record_token or production final-output association and must not be converted into one by copying numeric tags.
- Integration `src/godot/bridge/ecs_world.cpp:4351,4423,4450` has request/poll/take definitions, and `world_gpu_system_request.cpp:105` reaches the host/native System controller. The full `src` search at this snapshot finds only those EcsWorld declarations/definitions, no production WorldLoader/progress caller. That is a separate phase1 integration responsibility, not permission for a B4 shadow scheduler.

Shared ANGE main e00103f exposes only qualification start in its recorder header. The System additions above are in the separate active integration owner tree and are not present in that main tree. The previous committed recorder/header change visible in integration history is 00656f54a9c511eab138eee69ce97a257c05aa23 (Method V2); its current System additions are dirty. Do not describe dirty System work as an available accepted source commit.

## Missing B4 production joins

The scoped main/integration source searches found no definitions of `FinalOutputSourceRef`, `U04FinalRootSelectionRef`, `PublishedSnapshotRef`, `SnapshotBundle`, `OriginalOperationRef`, `SourceBridge` or `record_token`. Names alone are not a proof of semantic absence; the concrete route inspection above also found no carrier joining the whole selected E05 root/count/id/layout/provenance, U04 accepted-step selection, immutable C02 parameters, E20 resources, camera/host/material/history tuple and K output lease. Godot has no six `particles_external_*` RS/storage entry points yet, and no actual token→original E09 association table.

OUTPUT_CONTRACT §1/§5 and HISTORY_SOURCE_CONTRACT §1 require that complete retained association. A `StoragePartBorrow` plus scalar `SystemRecordProvenance` supplies useful exact backing/record primitives but cannot fill those other obligations. A qualification/System scalar ticket, copied source tag, string token or new issuer is not an implementation of that missing join.

## Next independent work and ownership boundary

The next source-authority-independent implementation slice is the already planned K-owned `ExternalOutputStorage` resource-prefix/lifecycle component: use the existing config/layout helpers and the Core-only create/disposal witnesses, retain every created prefix until actual return, and test exact failures/Unknown/no-next-draw maintenance. Public acquire/seal success must stay unreachable until the original owner supplies its real selected-source/operation/lease association and budget admission; do not invent an operation capability to make a test green. Actual GPU guard preparation and the queued 17 engine tests are also independent validation work once the build queue permits them.

The real E09 record-token bridge and positive renderer adoption remain pending original owner implementation/intake. This is an item dependency, not completion or blockage of the whole Godot queue. No source fix was attempted in another owner's dirty tree.

Temporary bytes deleted: 0 (no temporary executable/cache created). Retained handoff: this INVENTORY.md and SOURCE_SNAPSHOT.json only, for root's next source-integration decision.
