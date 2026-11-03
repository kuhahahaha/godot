# GPUParticles3D velocity-limit curve GPU motion synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `c3bc8afa549d5f53544cacbb52588fa6136f20327c0a71c83e4ab4fbae68aec6`.

A fixed-seed one-shot GPUParticles3D node used initial +X velocity 5, zero gravity and spread, and `ParticleProcessMaterial.velocity_limit_curve`. Each stage restarted the same particle node and read the production RD particle buffer after 30 frames. The curve sequence was 0.1, same-gradient in-place 1.0, replacement 0.1, then detached-old gradient changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0, returned eight finite active records and 1024 bytes in every stage, and reported identical mean X motion `0.0509804 → 0.5 → 0.0509804 → 0.0509804`; mean X velocity was `0.101961 → 1.0 → 0.101961 → 0.101961`. No engine or script error appeared in either log.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-velocity-limit-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. This is a synchronized readback after each stage; it proves the GPU simulation's sampled velocity limit and resource synchronization on this host, not an unsynchronized throughput or stall guarantee.

Other particle nodes, rendering methods/platforms and product ticket correlation remain open. No production source changed.
