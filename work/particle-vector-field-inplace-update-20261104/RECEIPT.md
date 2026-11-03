# Vulkan Mobile in-place 3D vector-field texture update

Date: 2026-11-04. Godot fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`.

The prior live vector-field probe covered texture replacement, clear and rebuild. This follow-up keeps the same `ImageTexture3D` object and RID attached to an active `GPUParticlesAttractorVectorField3D`, then calls `ImageTexture3D.update()` to replace both RGBA8 slices from +X to −X. It restarts the same seeded GPUParticles3D system after each stage and reads eight production RD particle transforms after 35 fixed 60 Hz frames.

| Stage | Active records | Mean X |
| --- | ---: | ---: |
| Before update, +X texture | 8 | 2.03996872901917 |
| After in-place update, −X texture | 8 | -2.27996516227722 |

`python3 work/particle-vector-field-inplace-update-20261104/run.py` returned rc 0 with 11/11 checks true. Both stages yielded 1024 bytes and finite active records; the same resource and RID checks were true. The runner verified the fixed editor hash before and after launch, Vulkan Forward Mobile, and no engine/script errors. Probe SHA-256 `274e36042a3c5ed9fbec9665aa70ad200955bf4ac4da9d1170b7142be0d25ee2`; raw log SHA-256 `71afcd6bc41f0aa9edcf7275e9147c5233e4609298061b59cded0edc453e3c6d`. Exact command, stage values, hashes and predicates are in `result.json`; raw output is `run.log`.

This verifies in-place texture bytes reach particle motion through the production Vulkan Mobile path. It does not prove spatial interpolation, other renderers, or the separate product six-link ticket gate. No production source or fixed editor changed.

## Visible Metal Mobile control

The same fixed editor and unchanged probe were run with `--rendering-driver metal --rendering-method mobile --fixed-fps 60` in a visible window. The process exited 0; the log identifies Metal 4.0 Forward Mobile. Both stages returned eight finite active records and 1024 bytes. Mean X was again `+2.03996872901917` before and `−2.27996516227722` after `update()`; the same resource and RID flags were true. No `ERROR:`, script error, seal error, or crash was present. Raw `metal.log` SHA-256 `bf156f8f99f6bfff765f1b65deee4a9837bebd539390b13784595e5d3f02b9ab`. This extends the in-place sampling result to Metal Mobile on this host; spatial interpolation and product ticket correlation remain separate.
