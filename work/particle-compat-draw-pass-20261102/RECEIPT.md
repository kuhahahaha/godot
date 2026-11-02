# Compatibility particle draw-pass resize runtime receipt

Date: 2026-11-02. Source change: `efd2a6f8e7`.

`scons tests=yes platform=macos target=editor -j4` completed with rc 0. The editor binary SHA256 is `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d`. This build used the current mixed worktree and verifies the integrated source state, not an isolated checkout.

`python3 work/particle-compat-draw-pass-20261102/run.py` completed with rc 0 and 6/6 checks. The live window initialized OpenGL 4.1 Metal Compatibility. At draw-pass counts 1, 2, and 1, respectively, the probe observed red/green pixels of 690/0, 200/680, and 690/0; AABB X size was 0.652188 in each stage. The particle node was freed and the log contained no engine or script errors. `RESULTS.json` records the command, binary hash, exit status, and observations.

The first probe run failed because overlapping quads made the second pass cover the first. The probe then offset the red and green quads so both passes could be observed independently; the final run passed.

The separate source-extracted `particle_gles3_draw_pass_count` fixture passed 4/4 against HEAD. The runtime probe verifies legal resize, visible draw-pass behavior, and cleanup. It does not directly observe the internal dependency notification or exercise invalid 0/5 counts on the GPU backend.
