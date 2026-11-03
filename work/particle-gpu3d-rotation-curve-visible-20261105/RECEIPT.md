# GPUParticles3D rotation-velocity curve GPU transform synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `a4b686e21cbdfa13e506514e6442d6eff86d115ec2bbe10d4de781532b9550b5`.

A fixed-seed one-shot GPUParticles3D node used zero gravity and velocity, enabled 3D rotation velocity at `(0,0,1)`, and attached `ParticleProcessMaterial.rotation_velocity_3d_curve`. The same node restarted for each stage. The curve sequence was zero, same-gradient in-place one, replacement zero, then detached-old gradient changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0, returned eight finite active records and 1024 bytes at every stage, and produced mean absolute off-axis basis component `0 → 0.0174531 → 0 → 0`. The latter two stages show the replacement remained authoritative after old-resource mutation. Neither log contains an engine or script error.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-rotation-curve-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. The production RD readback is synchronized; this verifies sampled GPU transform behavior and resource synchronization on this host, not unsynchronized throughput or stall behavior.

Other parameter curves, particle nodes, rendering methods/platforms, and product ticket correlation remain open. No production source changed.
