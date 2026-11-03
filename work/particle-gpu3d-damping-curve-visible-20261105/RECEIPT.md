# GPUParticles3D damping curve GPU motion synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `29db8ade4bb6d904cf7165daf3c35cc40a1bb28ab0e3a37335092ad44e2679d3`.

A fixed-seed one-shot GPUParticles3D node used initial +X velocity 5, zero gravity and spread, and fixed damping 8 multiplied by `ParticleProcessMaterial.PARAM_DAMPING` texture. The same node restarted for each stage. The curve sequence was zero, same-gradient in-place one, replacement zero, then detached-old gradient changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0 and returned eight finite active records (1024 bytes) in every stage. Mean X displacement was `2.5 → 1.4333330 → 2.5 → 2.5`; mean X velocity was `5 → 0.9999994 → 5 → 5`. Neither log contains an engine or script error.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-damping-curve-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. The production RD readback is synchronized; this verifies sampled GPU simulation behavior and resource synchronization on this host, not unsynchronized throughput or stall behavior.

Other parameter curves, particle nodes, rendering methods/platforms, and product ticket correlation remain open. No production source changed.
