# CurveXYZTexture content revision receipt

Date: 2026-11-05
Commit: d25a7261fd

## Change

`CurveXYZTexture` now exposes a monotonic `content_revision`. Width changes, replacement of any X/Y/Z curve, and mutations delivered through child `Curve::changed` callbacks advance the revision before rebuilding the texture. This makes curve-backed particle resources observable by typed resource synchronization in the same way as `CurveTexture`.

## Verification

- Added `[ResourceSnapshot] CurveXYZTexture revision tracks wrapper and child mutations` in `tests/scene/test_resource_snapshot_bridge.cpp`.
- `git diff --check` passed for all three changed files.
- Full Godot build/test was not started in this turn because the shared checkout had approximately 589 MiB free and other owner work was active; no build claim is made.
