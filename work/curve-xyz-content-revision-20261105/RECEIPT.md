# CurveXYZTexture content revision receipt

Date: 2026-11-05
Commit: d25a7261fd

## Change

`CurveXYZTexture` now exposes a monotonic `content_revision`. Width changes, replacement of any X/Y/Z curve, and mutations delivered through child `Curve::changed` callbacks advance the revision before rebuilding the texture. This makes curve-backed particle resources observable by typed resource synchronization in the same way as `CurveTexture`.

## Verification

- Added `[ResourceSnapshot] CurveXYZTexture revision tracks wrapper and child mutations` in `tests/scene/test_resource_snapshot_bridge.cpp`.
- `git diff --check` passed for all three changed files.
- Full Godot build/test was not started in this turn because the shared checkout had approximately 589 MiB free and other owner work was active; no build claim is made.

## Follow-up coverage

Commit `fe04b18672` extends the regression to X, Y, and Z child curve replacement. `git diff --check` passed. Full build remains deferred while shared owner work and low disk space are present.

## Detach coverage

Commit `0d0a1c736a` verifies that after replacing/removing child curves, mutations to the detached curve no longer advance the texture revision. `git diff --check` passed.

## ABI boundary coverage

Commit `$(git rev-parse --short HEAD)` records that the current V1 texture snapshot ABI intentionally rejects `CurveXYZTexture` as an unsupported wrapper type. Its revision is available for future typed ABI work without silently changing the frozen source-kind contract.
