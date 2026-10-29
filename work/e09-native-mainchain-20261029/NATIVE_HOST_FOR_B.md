# B original E09 native-host handoff

This is the single current handoff for the existing native host, actual APIs and limits. B owns actual E09 callee/input/production assertions in W1; D owns native host/RT. No A-tree build or source edit is needed to read/reuse these inputs. CURRENT_PINS and the numeric package pins distinguish current sources, private native DSO and older product DSO.

## Exact existing source and launch

Root: `/Users/macbookair/Documents/Work/godot_ANGE`. Control directory: `work/native-numeric-unit-correlation-20261029/`.

- `probe.cpp::numeric_control_init` loads real native procs from original Godot lookup, bootstraps actual library generation, initializes the normal godot-cpp SDK, registers original initializer/terminator and V2 SceneReady callback requiring BASE+RD_PAIR/main qualification. Guard configuration and actual Binding cleanup are retained. Its narrow lookup observer intercepts only actual after_binding_deinit to return the control's additional original CodeHold at the real Binding terminal, then forwards the real API; it does not fabricate session/registry/qualification lookups or return values.
- `initialize` runs at Scene level, borrows actual initialization frame/library/Binding cleanup action and acquires actual CodeHold. Prewarms Engine; normal SDK owns its own qualification CodeHold.
- `ready(context,scene_plan)` validates real SceneTree ID and retains the original SDK Tree wrapper. It runs synchronously on main RT for `--render-thread safe`, using real main RD binding/open bridge before native operations. It does not call local-RD submit/sync or manufacture a Host attachment.
- `runtime/probe.gd` dynamically loads `res://probe.gdextension` from SceneTree `_initialize` after Godot runtime initialization, then unloads through the actual manager. Do not autoload it via `.godot/extension_list.cfg` before SceneTree readiness.
- `runtime/probe.gdextension`: entry `numeric_control_init`, compatibility4.7, guarded exit opt-in1; both Debug/Release library keys point to private probe.dylib.
- `PROBE_BUILD_COMMAND.json`: exact clang++17 private DSO link, 44 existing original object files plus one original SDK archive (45 link inputs total); exported symbol limited to `_numeric_control_init`. `run_control.py --build` invokes this saved command only, never SDK/SCons/A builder. B's extra E09 TU closure is not included or asserted linked by this numeric fixture.
- `POSITIVE_COMMAND.json`: exact non-headless Metal/mobile/safe command with `--script probe.gd` and `ANGE_MACOS_BACKGROUND_RENDER=1`, NUMERIC_CONTROL_INPUT/OUTPUT pointing to this control's retained inputs/own result directories. The ordinary core source build is `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes`. One builder per tree.

Frozen reproduction: `python3 work/native-numeric-unit-correlation-20261029/run_control.py --build` is valid only when current editor/source/object hashes match its frozen inputs. Native tests reject a changed core against NATIVE_GENERATION_RECEIPT; do not overwrite that evidence or relabel it current after a rebuild. For a changed editor, use a separate fixture output directory and run original factory export/source/SPIR-V/interface validation plus generate_native.py before linking/running. Prior current-control source/compiler/export pins are in ARTIFACT_ORIGINS and r2 EXPORT_* records.

## Genuine native APIs already available

`GDExtensionBinding::InitObject::register_scene_ready_callback_v2` and `require_main_rd_qualification` provide actual Ready context/plan; `GDExtensionBinding::borrow_qualification_code_hold(ready,...)` exposes the SDK's original hold to A during that frame. Native `device_session_bind_main_scene_ready_v1` returns actual DeviceRegistryContextRef and OriginalDeviceSessionRef; `device_session_qualification_open_v1` authenticates same ready/device/session/original Binding CodeHold. These are real core refs, not numeric fixture lookup values.

The control uses actual qualification RT configure/reserve/activate/observe/release and original ticket/facts/copy/cancel/release/physical-return APIs through those refs. Safe RT is validated. Its direct uniform_set_create is an actual main-RT call; the separate-RT fixture variant previously refused before record, so do not claim separate-RT recorder support.

These raw C POD refs/bridge are not the A C++ `OriginalDeviceSessionRef::Binding`, E06 `OwnerObservationLease`, `NativeHold` or `QualificationScope`. Actual wrappers must come from original A factories.

## Original A/B route and current exact boundary

Required actual route: HostControlService::service_children → loader.service_qualification(records,storage,same meter,allow_new) → storage.start_qualification → StageExecutor::start_native_qualification → E09NativeRecorder::start/service/cancel/drained. All remain under actual main RT/session/registry/same Store.

Original parent route is request_waiting_native(context, original OwnerObservationLease, original PrepareRequestKey,...) → retain_qualification_parent → request_qualification → next_qualification_scope. HostAttachmentInstaller::bind_main_device installs the concrete GodotNativeUnitPort, binds qualification context and commits session observer only after real storage/main_pair are installed. retain_session_observer then issues the existing observed pin; original Scope requires that parent observer at new-work.

Current `HostAttachmentInstaller::install_main_storage_candidate` reaches actual bind_main_scene_ready with real SDK hold, then `read_storage_owner_inputs()` unconditionally refuses at host_attachment.cpp521 before pair/storage publication at586–587. It cannot commit observer. The lifecycle test installer calls this same rejecting transaction. MainRDStorageTestAccess/ControllerFixture use scripted native/Binding owners and cannot be promoted by running inside Godot. There is no current callable original source accepting explicit test-owned E05 storage/maintenance budgets while committing this original Host pipeline. install_preparation accepts preparation limits only; raw QRT limits are not E05 owner inputs.

Thus the initialized host solves the standalone RID initialization issue, but cannot currently supply B with a genuine committed Host observer/Scope. The exact needed A endpoint is an original Host-owned installation route retaining actual SceneReady/SDK hold/session/context and issuing existing observer/Store Scope with explicit test-owned storage/maintenance budgets (or genuine production input). D has not created a replacement issuer or relaxed read_storage_owner_inputs. It is not valid to fill this gap with the numeric unit digest.

## B assertions to run once that endpoint exists

Preoccupy E05 capacity under explicit test-owned limits; actual start must retain the same R01 attempt with Empty allocation; release real lease and service finite same meter to admit on the same attempt. In a second BUSY branch cancel/close must issue no new E05 part, transition through Returning/Empty to drained, restore original reservation/account charges and return actual RID/holder owners. B owns these callee assertions and exact inputs; D's host must retain original callback/SDK/library lifecycle through drain. No existing fixture result signs these assertions yet.

## Actual completed evidence

Commit24df5f5d14: original factory Metal/safe positive13known facts/possible0/exact104-byte owned readback; cancel-prefix3facts/possible0/no request or readback; actual2unit/8buffer physical returns; all reply/ticket/bridge/session/context acknowledgements; actual SDK Binding and manager unload0. Both184-byte reports contain actual nonzero retained unit digest;77 run pins stable and45 compiled inputs frozen. Independent review: INDEPENDENT_REVIEW.md. Scope excludes actual E09/Host/Store issuer, whole corpus/two-stage Verified, separate RT, Ready/G1 and OS unmapping.
