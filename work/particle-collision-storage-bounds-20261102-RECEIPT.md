# Particle collision storage setter bounds receipt

Date: 2026-11-02. Base before this change: `baefc6dafc`.

The RD and GLES3 particle collision storage setters now reject invalid collision type, non-finite or negative sphere radius, non-finite attractor strength, directionality outside 0..1, attenuation outside 0..8, and non-finite instance transforms before changing stored state. GLES3 box extents also reject non-finite or negative components; the RD box extents guard was already in HEAD. The staged source contains exactly 13 inserted guard lines across these setter functions; the unrelated dirty renderer changes remain unstaged.

`python3 tests/servers/rendering/fixtures/particle_collision_setter_bounds/run.py --source-ref HEAD` failed on the missing RD type guard before staging. The worktree fixture passed 14/14 source checks. This fixture proves guard placement in source; it does not execute RD or GLES3 collision rendering. A mixed-worktree editor build exists, but this receipt does not claim it as an isolated build of the staged patch.
