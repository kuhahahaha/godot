# Current Metal host run — 2026-11-01

Binary `bin/godot.macos.editor.arm64` at source `0eac04f63e` ran the existing ordinary GPUParticles fixture through the normal macOS main loop:

- Backend: Metal 4.0, Forward+, Apple M3 Pro (Apple9), safe render thread.
- Runtime reported `main_rd=true`, editor feature true, editor hint false, shader cache disabled.
- `normal`, `fractional`, `preprocess`, and `zero-velocity`: each exited rc0 with 4 stages, 288 pixel checks, and 36/36 fresh marker checks passing.
- `omit-second-request`: exited rc1 with exactly 18 expected particle-position failures; marker checks remained 36/36.
- Normal run saved `initial.png`, `paused_hold.png`, `requested_1.png`, and `requested_2_resize.png` under `/tmp/particle-metal-current-20261101/png/`.

This is real Metal scene rendering and frame-post-draw freshness evidence for the ordinary particle path. It does not prove injected production upload failure, same-request readback, physical retirement, K03 typed-close, Ready, or G1.
