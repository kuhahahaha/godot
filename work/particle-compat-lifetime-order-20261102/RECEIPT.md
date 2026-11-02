# Compatibility particle lifetime draw-order runtime receipt

Date: 2026-11-02

`SCons tests=yes platform=macos target=editor -j4` completed with rc 0 after `ab4566dbb5`. The resulting editor binary SHA256 is `546257261d049766ac6d2223ff9e79b58797112de03b2652d0dd01d040803695`. This build used the current mixed worktree, so it verifies the integrated source state rather than a clean isolated checkout.

`python3 work/particle-compat-lifetime-order-20261102/run.py` ran OpenGL 4.1 Metal Compatibility with fixed 60 FPS. It passed 6/6 checks: `DRAW_ORDER_LIFETIME` rendered 690 red pixels with positive AABB spread; `DRAW_ORDER_REVERSE_LIFETIME` rendered 680 red pixels with positive AABB spread; the particle node was freed; no engine or script errors appeared; process rc 0. `RESULTS.json` fixes the command, binary hash and observations. `[ParticlesReadback]*` passed 11 cases / 58 assertions. The committed lifetime split source fixture passed 5/5 against HEAD.

This qualifies ordinary live lifetime ordering and cleanup. It does not force invalid phase/amount inputs through the GPU path; those negative branches retain source-level evidence only.
