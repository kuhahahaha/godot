# Particle collision and heightfield fixture recheck

Date: 2026-11-06

Focused source-extraction and harness checks ran against the current Godot worktree while the shared `scons test -j4` build remained active. No production source changed in this recheck.

## Results

- `particle_collision_scene_bounds/run.py`: **9/9** checks passed.
- `particle_heightfield_size/run.py`: **18/18** float and **18/18** double checks passed.
- `particle_gles3_heightfield_size/run.py`: **8/8** checks passed.
- `particle_rd_heightfield_extents/run.py`: **11/11** checks passed.
- `particle_rd_heightfield_failure/run.py`: **2/2** checks passed.
- `particle_gles3_heightfield_failure/run.py`: **10/10** checks passed.

The checks cover collision scene bounds, finite/degenerate heightfield sizing in both precision modes, GLES3 sizing, RD extent-cache invalidation, and RD/GLES3 failure handoff behavior. They are source/harness evidence only; they do not claim live GPU execution, product six-link acceptance, or completion of the shared full build.

## Environment

- Worktree: `/Users/macbookair/Documents/Work/godot_ANGE`
- Existing shared build: `scons test -j4`, PID `33695` at inspection time.
- Available disk: approximately 1.6 GiB.
