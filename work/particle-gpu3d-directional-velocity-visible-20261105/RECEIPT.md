# GPUParticles3D directional-velocity curve GPU motion synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `b99ca8596897ac084cc2b65dca048bdee6547dbe0ae31bd1f18c3bf116d033f0`.

A fixed-seed one-shot GPUParticles3D node used zero initial velocity and gravity, fixed directional-velocity multiplier 2, and `ParticleProcessMaterial.PARAM_DIRECTIONAL_VELOCITY` with a `CurveXYZTexture`. X was varied while Y/Z stayed zero. The same node restarted for each stage. The X curve sequence was zero, same-resource in-place one, replacement zero, then detached-old X curve changed to 0.5. Both visible Metal Mobile and Vulkan Mobile exited 0 and returned eight finite active records (1024 bytes) in every stage. Mean X displacement was `0 → 1 → 0 → 0`; mean X velocity was `0 → 1.9999999 → 0 → 0`. Neither log contains an engine or script error.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-directional-velocity-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are beside this receipt. The production RD readback is synchronized, so this proves sampled GPU simulation behavior and resource synchronization on this host, not unsynchronized throughput or stall behavior.

Other parameter curves, particle nodes, rendering methods/platforms, and product ticket correlation remain open. No production source changed.
