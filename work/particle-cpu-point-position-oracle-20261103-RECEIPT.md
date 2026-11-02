# CPU 3D point position oracle, 2026-11-03

`tests/scene/fixtures/cpu_particle_point_seed/positions_3d.gd` exercises a fixed-seed `CPUParticles3D` node through the real SceneTree, forces its render-thread upload, and reads the public MultiMesh buffer via `RenderingServer.multimesh_get_buffer(p.get_base())`. With 32 distinct source points and a 2-second process request, four active particle X positions have the literal expected sequence `[20,20,16,24]`.

The current editor SHA-256 `102c854f24fb15409eb3991ee04c6a6898341dd8eb69e6f3719b20a4a94a2080` ran the fixture in three separate processes: each exited 0 and reported `matches:true` with that exact sequence. The retained pre-fix editor SHA-256 `f24a96ffdce2f92abd2d92cc0522d46812c6ab718f0bb882181f6e14af307639` ran it in three separate processes: each reported `matches:false`, with sequences `[26,2,2,30]`, `[27,32,13,4]`, and `[13,16,4,12]`. The direct pre-fix invocation exited 1. This verifies that the fixture detects the former global-RNG point selection and that the fixed-seed 3D output is stable on this host and binary.

Command: `EDITOR --headless --path tests/scene/fixtures/cpu_particle_point_seed --script positions_3d.gd`, replacing `EDITOR` with either recorded editor path. This is CPU 3D instance-buffer evidence only. It does not prove 2D pixel output, GPU emission, cross-platform identity, or an ANGE product request.
