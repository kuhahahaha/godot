# GPUParticles3D radial-velocity curve GPU motion synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `1ecf5835192d1dbabe55e462682315440a7ba07fc2b91c38dd3155b952f66c00`.

A fixed-seed one-shot GPUParticles3D node emitted eight particles from a unit sphere surface with zero initial velocity and gravity. The fixed radial velocity was 2, multiplied by `ParticleProcessMaterial.PARAM_RADIAL_VELOCITY` texture. The same node restarted at each stage. Curve values were zero, same-gradient in-place one, replacement zero, then detached-old gradient changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0 and returned eight finite active records (1024 bytes) in every stage. Mean X position was `0.4201578 → 0.8403155 → 0.4201578 → 0.4201578`; mean X velocity was `0 → 0.8403155 → 0 → 0`. The nonzero baseline X position comes from the fixed-seed sphere emission positions, so the velocity assertion distinguishes curve-driven motion. Neither log contains an engine or script error.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-radial-velocity-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. The production RD readback is synchronized; this verifies sampled GPU simulation behavior and resource synchronization on this host, not unsynchronized throughput or stall behavior.

Other parameter curves, particle nodes, rendering methods/platforms, and product ticket correlation remain open. No production source changed.
