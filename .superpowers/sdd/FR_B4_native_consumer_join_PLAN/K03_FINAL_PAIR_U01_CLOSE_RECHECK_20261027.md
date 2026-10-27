# K03 final current-pair VFX/U01/close recheck — 2026-10-27

## Scope and result

This is the final non-headless fixed-pair recheck after the last completed DSO
build observed in the shared ANGE worktree. It runs the VFX fixture, the
no-VFX control, and an explicit `EcsWorld::exit_world()` close sequence with
the same editor and DSO. All three probes return rc0. VFX source-stage loading,
world entry, explicit world exit, node disposal, and extension unload are
observed. No operation, packet, completion, exact readback, physical GPU
disposal, Ready, Verified, or GPU qualification claim is made.

## Exact final pair and build provenance

- Godot editor SHA-256: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- Final ANGE DSO SHA-256: `149342a6a099199bc345bd2e23844491f58485f02ef2fc8529dd7da4fe0811f7`
- ANGE source HEAD at final probe: `b4a8924322454a2f153aab2437a721577677d881`
- Shared worktree has unrelated dirty files and a dirty `src/godot/loader/world_loader.cpp`; source hashes for the VFX loader/runtime files are listed in the adjacent JSON.
- Host: non-headless Metal/mobile, Apple M3 Pro.

The final DSO build log ends with the shared-library link and postlink code-sign
step, and direct `codesign --verify --verbose=0` on the final DSO returned rc0.
The detached build shell did not preserve a separately readable exit code in
this turn; the earlier `e7ae9ffbd` receipt's rc0 belongs to the prior DSO
`0e3f295d…` and is not attributed to this final `149342a6…` product.

## Final VFX probe

`K03_FINAL_PAIR_U01_CLOSE_RECHECK_20261027_VFX.log`:

- command: `bin/godot.macos.editor.dev.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path /tmp/k03-current-pair-u01-current-20261027 --script res://probe.gd`
- rc `0`
- `U01_EXTENSION_STATUS=0`, `U01_EXTENSION_OK=true`
- `[vfx] runtime sources ready entries=2 systems=1`
- `U01_ENTER_WORLD=true`
- `U01_UNLOAD_STATUS=0`, `U01_UNLOAD_OK=true`

## Final no-VFX control

`K03_FINAL_PAIR_U01_CLOSE_RECHECK_20261027_NOVFX.log`:

- command: same editor/Metal/mobile invocation against
  `/tmp/k03-current-pair-u01-current-novfx-20261027`
- rc `0`
- `U01_EXTENSION_STATUS=0`, `U01_EXTENSION_OK=true`
- `[vfx] runtime sources ready entries=1 systems=0`
- `U01_ENTER_WORLD=true`
- `U01_UNLOAD_STATUS=0`, `U01_UNLOAD_OK=true`

## Final explicit close probe

`K03_FINAL_PAIR_U01_CLOSE_RECHECK_20261027_CLOSE.log` uses the new close-only
probe and the same VFX fixture:

1. load the extension;
2. instantiate `EcsWorld` and enter the VFX world;
3. record `is_world_active()` before close;
4. call `world.exit_world()` and yield one frame;
5. require `is_world_active() == false`;
6. free the node and unload the extension.

Observed rc `0`:

```text
CLOSE_EXTENSION_STATUS=0
CLOSE_ENTER_WORLD=true
CLOSE_ACTIVE_BEFORE=true
CLOSE_ACTIVE_AFTER_EXIT=false
CLOSE_UNLOAD_STATUS=0
CLOSE_UNLOAD_OK=true
```

The only warnings in all three logs are the known tree/flower authoring
default warnings. There is no `GDEXTENSION_CLOSE_FAILED` line and no extension,
world-load, or explicit-close error.

## Boundary and claim matrix

This recheck proves exact editor/DSO interface alignment, source-stage VFX
projection, world entry, explicit world exit, node disposal, and extension
unload for this fixture. It does not prove that the VFX projection is consumed
by a renderer packet owner or that a GPU particle operation was issued.

U08 producer admission, E09 same-request source/operation association,
operation, completion, readback authority/bytes, packet publication, physical
disposal, Released, Ready, Verified, and GPU qualification remain false.
The local queue can continue independently while the external owner supplies
those inputs.
