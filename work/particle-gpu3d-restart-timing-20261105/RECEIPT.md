# GPUParticles3D restart one-shot timing

Date: 2026-11-05

## Reproduction

Command (from repository root):

```sh
bin/godot.macos.editor.arm64 --headless --fixed-fps 60 --path . --script work/particle-gpu3d-restart-timing-20261105/probe.gd
```

Pinned pre-change editor SHA-256: `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; matching sidecar SHA-256: `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`.

RED: exit 1. At fixed 60 FPS, frames 30 and 45 have 3D `restart()` `emitting=false` while same-parameter 3D `set_emitting(true)` and 2D `restart()` remain `true`. The script runs normally and emits `RESULT FAIL`; see `red.log` and `red.rc`.

## Change

In `GPUParticles3D::restart()`, set `emission_time = lifetime`, matching the other one-shot entry paths. Keep `active_time = lifetime * (2 - explosiveness_ratio)`.

## Verification status

Source-matched build: `scons -Q tests=yes platform=macos target=editor extra_suffix=restart_timing_probe -j2`, rc 0. The build log contains `Compiling scene/3d/gpu_particles_3d.cpp` and links `bin/godot.macos.editor.arm64.restart_timing_probe`; probe editor SHA-256 `9eb089b7a7343c794ab748ecceff1339466de20eb425706fec785b9a7ab3e3b9`, sidecar SHA-256 `995181b178676657f49744e432dd9b7911fddd784d1d1f3abf0301ac792af28d`.

GREEN: the same script under the probe editor exits 0. All three emitters remain `emitting=true` at frames 30 and 45, then `false` at frame 90 with exactly one `finished` signal each. See `green.log` and `green.rc`.

Neighbor test: `--headless --path . --test --test-case='[GPUParticles]*'`, rc 0, 25 cases and 138 assertions passed. See `neighbor.log` and `neighbor.rc`. `git diff --check -- scene/3d/gpu_particles_3d.cpp` exits 0.

The pinned editor and sidecar retain their original hashes. This is a headless SceneTree timing check; it does not establish particle pixels or GPU rendering output.
