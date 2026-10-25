# Current pair U01/VFX and R01 negative receipt

Date: 2026-10-25 (Asia/Shanghai)

This receipt fixes the current source and binary pair used for the continuation.
It is a fail-closed negative result; it does not claim U01 Ready, R01, R02, or
K01 qualification.

## Pair identity

- Godot HEAD: `cb6e9a4d57504f0eb52fa18da02abb674d64363d`
- Godot editor SHA-256: `309e8c41366b93ad4b6554ff3f1722663e840e3c05fe9212b12ea762bb318ae4`
- ANGE HEAD: `133c63cd3509261585a667e6a640b9ad0dbd47a5`
- ANGE DSO: `/Users/macbookair/Documents/Work/ANGE/bin/libange.macos.template_debug.dev.arm64.dylib`
- ANGE DSO SHA-256: `8faf38c594c5685be56a1e6822a88017d6055d63f3ef924928afdcfac8719892`
- Host: non-headless macOS, Vulkan 1.2.357, Forward Mobile, Apple M3 Pro

Both source trees are mixed dirty worktrees. No unrelated source was reverted,
cleaned, or staged for this receipt.

## Canonical U01/VFX probe

The probe was run with the current DSO through the existing guarded fixture,
using a temporary filename symlink only when the fixture expected the historical
`.template_debug.dev` name; the symlink was removed after the run. The observed
run is recorded in `/tmp/k03-current-u01-vfx.log`.

Observed result:

```text
U01_EXTENSION=0
[vfx] VFX-TOOL-001 owner_unavailable: U02 System semantic issuer is not connected
[WorldLoader] resource stage failed: vfx
[world_load] rollback at load_resources:vfx
U01_ENTER_WORLD=false
U01_UNLOAD=1
GDEXTENSION_CLOSE_FAILED error=030 route=5
```

The process returned `rc=78`. The first qualification boundary is the A-side
U02 semantic issuer, before GPU execution or native completion. The later close
error is a fail-closed consequence of the existing runtime-holder seam and is
not evidence that VFX executed.

The independent no-VFX control reached `U01_ENTER_WORLD=true` with the same
pair, then still stopped at the known holder close boundary (`rc=78`); this is
recorded in `/tmp/k03-current-u01-no-vfx.log` and does not substitute for the
missing VFX issuer.

## C provenance consumer audit

C commit `221c07170f9c3f22c49f6deaaa6a22328b58ae9b` exposes
`RecordSession::inspect_retirement_provenance()`, returning the owner-issued
`{CanonicalRange, generation}` view. Its receipt is
`/Users/macbookair/Documents/Work/ANGE_w2/work/r01-current-source-20261025/evidence/RECEIPT.json`.
The explicit D consumer handoff is commit `1979c81c8c71f28540361e637f8d2f06dd0c54c0`,
artifact `D_CONSUMER_HANDOFF.md` (SHA-256
`5c910485fa5aca9140e6c7c1938e9121368f6b5e3014a39bd5e239d96404f20c`).

The current D source has no production caller that consumes this view at a
RenderingDevice graph producer. The D-side audit still finds:

- no original authenticated R01 operation key or reservation passed into the
  production graph/dispatch callers;
- no checked append result with `known_not_issued`, `issued`, or
  `unknown_side_effect`;
- no handle-to-native issue/completion association keyed by the same request;
- no exact same-request range readback and no K01 completion issuer.

Therefore the C provenance view remains an owner observation only. D must not
mint a replacement token, infer identity from a graph handle/RID/session/fence,
or claim completion/readback/Ready. The precise adapter minimum remains in
`R02_OWNER_ADAPTER_MINIMUM_20251025.md`.

## Verification commands

```text
sha256sum /Users/macbookair/Documents/Work/ANGE/bin/libange.macos.template_debug.dev.arm64.dylib
git -C /Users/macbookair/Documents/Work/ANGE rev-parse HEAD
git -C /Users/macbookair/Documents/Work/godot_ANGE rev-parse HEAD
-- non-headless Vulkan/mobile canonical U01 VFX probe -> /tmp/k03-current-u01-vfx.log (rc=78)
-- current focused regressions: *device_exit* 121/2444, [R02][Graph]* 5/66, [K03][P2]* 21/333, rc=0
```


## 2026-10-25 default-profile build control

The first rebuild used the default SCons profile and produced `/Users/macbookair/Documents/Work/ANGE/bin/libange.macos.template_debug.arm64.dylib` (SHA-256 `af894638c9d532caef5b853c1717cfee1f07cd5facd685c2d0e14d2d41325071`). The fixture still referenced `template_debug.dev.arm64`, so the immediately following rc79 probe loaded the pre-existing dev DSO. It cannot qualify or diagnose the default-profile image. The matching dev-profile rebuild and debugger diagnosis below supersede that attempted attribution.

## 2026-10-25 dev-profile initialization sequence diagnosis

A matching `target=template_debug dev_build=yes` DSO was built successfully, then tested with the fixed non-headless host command:

```text
/Users/macbookair/Documents/Work/godot_ANGE/bin/godot.macos.editor.dev.arm64 --path /tmp/ange-u01-runtime-20261025 --script res://probe.gd
```

Editor SHA-256 remained `309e8c41366b93ad4b6554ff3f1722663e840e3c05fe9212b12ea762bb318ae4`. The restored fail-closed dev DSO SHA-256 was `2947827115eea402d09045b230384485e762d188ac0627e0cbc81c33051dd77c`. The final run returned rc79; `/tmp/k03-current-u01-vfx-final-guard.log` contains `GDEXTENSION_LEVEL_DISPATCH_FAILED error=002 level=2 phase=I` before `U01_EXTENSION` or any VFX evaluation.

LLDB on the preceding matching dev build showed `initialize_ANGE()` failing at `src/register_types.cpp:187` because `SceneTree::get_singleton()` was null in the original SCENE initializer; the call chain was `particle_scene_failed` → `terminate_initialize_blocked` → `require_level_dispatch_success`, recorded in `/tmp/k03-init-lldb.log`. A temporary local diagnostic change removed that guard and was immediately falsified: the same initializer then reached `RenderingServer::get_singleton()->create_local_rendering_device()` at `src/register_types.cpp:207`, but the `RenderingServer` wrapper was unavailable and the process crashed (rc134; `/tmp/k03-current-u01-vfx-scene-fixed.log`). That diagnostic change was reverted and the dev DSO rebuilt before the final fail-closed run above.

The host ordering explains both observations: `Main::setup2()` calls `initialize_extensions(SCENE)` at `main/main.cpp:3844`; `register_server_singletons()` follows at line 3915; MainLoop/SceneTree creation follows in `Main::start()` around lines 4414–4490. Thus D currently asks for host wrappers before their legal publication time. Moving device creation and exact cleanup registration requires a real later owner/lifecycle seam. An absent SceneTree or RenderingServer is not permission to fabricate an ObjectID, binding, device, R01 key, or completion. U01/VFX and close acceptance remain unproven; G1 remains fail-closed.

## Later callback seam audit

The host does expose `register_main_loop_callbacks`: `GDExtensionBinding::InitObject::register_startup_callback()` stores a startup callback, `GDExtensionBinding::initialize_level()` registers it, and `GDExtensionManager::startup()` dispatches it after the main loop exists. This is a plausible timing seam, but it is not yet an admitted owner seam for this guarded DSO: its frozen guard configuration explicitly declares no main-loop entry class, and the callback itself is only a counted callback frame, not a finite BINDING cleanup action. The current DSO also has no mechanism to schedule a newly materialized local `RenderingDevice` wrapper on its exact original BINDING cleanup after SCENE initialization returns. Registering the callback or delaying device creation alone would therefore still fail G1, so no such unverified change was made. Required next integration is an authenticated later lifecycle action with exact object cleanup scheduling and preflight evidence, followed by a fresh same-pair runtime test.
