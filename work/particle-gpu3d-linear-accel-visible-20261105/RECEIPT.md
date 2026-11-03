# GPUParticles3D linear-acceleration curve GPU motion synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `c8406cba247aa6df9b0566d31cb0fbc1f0454f27b6477d1fa59912848bb6b8e1`.

A fixed-seed one-shot GPUParticles3D node used initial +X velocity 1, zero gravity and spread, and fixed linear acceleration 10 multiplied by `ParticleProcessMaterial.PARAM_LINEAR_ACCEL` texture. The same node restarted for each stage. The curve sequence was zero, same-gradient in-place one, replacement zero, then detached-old gradient changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0 and returned eight finite active records (1024 bytes) in every stage. Mean X displacement was `0.5 → 1.8333334 → 0.5 → 0.5`; mean X velocity was `1 → 6.0000005 → 1 → 1`. Neither log contains an engine or script error.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-linear-accel-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. The production RD readback is synchronized, so this proves sampled GPU simulation behavior and resource synchronization on this host, not unsynchronized throughput or stall behavior.

Other parameter curves, particle nodes, rendering methods/platforms, and product ticket correlation remain open. No production source changed.
