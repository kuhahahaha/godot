# K03 final2 current-pair VFX/U01/close recheck — 2026-10-27

This package records the last completed shared-worktree DSO build and the
single final probe set run with that product. It preserves the previous
`149342a6…` pair as historical final1 evidence and does not merge the two
provenances.

## Exact pair

- Godot editor SHA-256: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- Final2 ANGE DSO SHA-256: `a078f6dec6661d5fb5217c8b95c59d162f18ac5aa8bc81cef9e9c85c548fcb45`
- ANGE source HEAD: `53ff6aa1d48aa3697beff91eb31fee850ca326af`
- Host: non-headless Metal/mobile, Apple M3 Pro.

The formal successor receipt records SCons rc0, 3012 compile records, final link/postlink, and direct deep/strict codesign rc0; its product hash matches the final2 DSO.
The adjacent JSON records all source and raw-log hashes.

## Probe results

VFX log:

- rc `0`
- `U01_EXTENSION_STATUS=0`
- `[vfx] runtime sources ready entries=2 systems=1`
- `U01_ENTER_WORLD=true`
- `U01_UNLOAD_STATUS=0`, `U01_UNLOAD_OK=true`

No-VFX control:

- rc `0`
- `U01_EXTENSION_STATUS=0`
- `[vfx] runtime sources ready entries=1 systems=0`
- `U01_ENTER_WORLD=true`
- `U01_UNLOAD_STATUS=0`, `U01_UNLOAD_OK=true`

Explicit close probe:

- rc `0`
- `CLOSE_EXTENSION_STATUS=0`
- `CLOSE_ENTER_WORLD=true`
- `CLOSE_ACTIVE_BEFORE=true`
- `CLOSE_ACTIVE_AFTER_EXIT=false`
- `CLOSE_UNLOAD_STATUS=0`, `CLOSE_UNLOAD_OK=true`

No log contains `GDEXTENSION_CLOSE_FAILED`; only the known tree/flower
authoring-default warnings appear.

## Scope

This proves exact pair loading, U01 VFX source-stage projection, world entry,
explicit `exit_world()`, node disposal, and extension unload for these fixtures.
It does not prove U08/E09 association, operation, completion, readback bytes,
packet publication, external physical disposal, Released, Ready, Verified, or
GPU qualification. All those claims remain false.
