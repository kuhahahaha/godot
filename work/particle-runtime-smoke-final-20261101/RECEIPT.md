# Particle Metal runtime smoke — 2026-11-01

This is a Godot-native runtime regression smoke for particle processing and resource lifecycle. It is scoped to the current Godot source/binary and does not claim ANGE owner, typed-close, G1, or product qualification.

## Inputs

- Repository: `/Users/macbookair/Documents/Work/godot_ANGE`
- Source HEAD at invocation: `98d315818db624e4b81f867f718ec5fac9beb385`
- Binary: `bin/godot.macos.editor.arm64`
- Binary SHA-256: `f24a96ffdce2f92abd2d92cc0522d46812c6ab718f0bb882181f6e14af307639`
- Version: `4.7.2.stable.custom_build.98d315818`
- Build command: `scons platform=macos target=editor -j1`
- Build result: `rc=0`, 14.15 seconds, `servers/rendering/renderer_rd/storage_rd/particles_storage.cpp` compiled and editor linked.

## Runtime command

```text
./bin/godot.macos.editor.arm64 --path work/particle-runtime-smoke-final-20261101/project --rendering-driver metal --rendering-method forward_plus --fixed-fps 60 --verbose
```

Runtime result: `rc=0`. Device: Apple M3 Pro / Metal 4.0 Forward+.

The project creates a `Camera3D`, an unshaded red `QuadMesh` draw pass, fixed-seed `GPUParticles3D` with 64 particles, and a finite visibility AABB. It runs 230 frames and waits for post-draw synchronization before each readback.

Observed stages:

- `active`: `red_pixels=690`, AABB size `(0.652188, 0.16, 0.16)`
- `restarted`: `red_pixels=690`, AABB size `(0.652188, 0.16, 0.16)`
- `stopped`: `red_pixels=0`, AABB size `(0.16, 0.16, 0.16)`
- `resumed`: `red_pixels=690`, AABB size `(0.652188, 0.16, 0.16)`
- `freed`: `red_pixels=0`, AABB size `(0, 0, 0)` and `invalid=true`
- `PARTICLE_SMOKE_COMPLETE frames=230 failures=0`

PNG captures are stored beside the project (`active.png`, `restarted.png`, `stopped.png`, `resumed.png`, `freed.png`). The full output is in `run.log`; the exact return code is in `run.rc`.

## Source identity caveat

`source-after.json` records eight tracked source files that changed while the build/run window was open, including particle CPU/test files and physics wrapper files. This indicates concurrent worktree edits during the invocation. The binary identity and runtime evidence above are exact for the produced artifact; a later clean rebuild should be performed after all parallel source edits are frozen before using this as a final combined source receipt.

## Final node and CPU manual correction — 2026-11-01

Independent review (`ASTRA_PARTICLE_REVIEW.md`) found two actual CPU defects in the pending node increment: manual emission from a stopped node did not restore active/internal scheduling, and a rejected unprocessable request had already enabled redraw (and could refresh 2D interpolation). The old manual harness incorrectly synthesized active=true inside its simulation stub. The harness now removes that invented behavior, and the actual EDF runtime records four exact failures (`astra-review/native-red.json`).

The corrected production functions restore activity/scheduling only for actual positive manual emission, and validate all used time phases before redraw/interpolation/simulation side effects. Residual-only does not revive an idle system. Rejected update retains its existing pending request fields; observable simulation, redraw and scheduling state is preserved.

Unique incremental build rc0, source_drift=[] (`manual-build-result.json`). Immutable CPU host `ec55001baf673a0d9ae29b68ec5fe44d21098e5da8e7a5e183bc515add631e4d` and controlled metadata are pinned in `MANUAL_PINNED_HOST.json`. Actual same-probe GREEN preserves two expected progress-refusal errors and no failures (`manual-native-green.json`). Separate real lifecycle execution (`manual-lifecycle.json`) confirms two dimensions: combined manual emission finishes once and stops after 54 frames; one-shot emission finishes once and stops after 102; residual-only remains stopped; active-state refusal preserves processing/emitting/draw connection and physics-processing state. Changing 2D tree interpolation before refusal leaves it unconsumed; a following admitted request refreshes it. These are ordinary headless CPU observations, not pixel/GPU evidence.

All 14 source-extracted fixtures return zero (`final-source-fixtures.json`); the manual four precision/dimension variants each pass 22/22. Native node tests pass 14 cases/75 assertions on separately immutable formatted-physics host `1d20d3073b1284134b9ce4f16f4fa5ba8301a5f8baee5685eeebbd7e67228f2c`; Qualification neighbors pass 185/3141. The source hashes and exact host scopes are in `FINAL_SOURCE_PINS.json`. C++ clang-format and all four new Python fixtures' pinned Ruff0.15.8 lint/format checks pass.

The original GPU smoke/source-window limits above remain historical. No new GPU retry/trail or full ANGE product qualification is claimed by these CPU/native node checks.

Pinned Mypy1.19.1 also reports no issues for all four new Python fixture sources; each has the executable bit required by its shebang.
