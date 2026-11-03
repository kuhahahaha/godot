# GPUParticles3D emission-curve visible resource synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `1b40a1ced14bcfc034f8e3fa1bfc41269b04a91dbaac5d55d2b4f3e42219f56e`.

A fixed-seed GPUParticles3D node drew eight dim unshaded quads using `ParticleProcessMaterial.emission_curve`. In a 16×16 center ROI, both visible Metal Mobile and Vulkan Mobile observed 256 particle pixels throughout. Summed RGB luminance changed `51.200 → 73.286 → 51.200 → 51.200` as the retained curve changed 0→1 in place, was replaced by a 0 curve, and the detached old curve changed to 0.5. Both processes exited 0, saved all captures, and had no engine or script errors. Replacement and detached-old PNG SHA-256 match at `4aedc15b000ab4405b93d9c66c90917a2fb5a8d6f5f2a5a010dbdf5e6f6e1f0d`.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-emission-curve-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs and captures are beside this receipt. The PNGs correspond to the final Vulkan run because the output directory was reused; per-driver observations remain in the logs. An initial Metal probe run failed its assertion because it summed the background across the entire viewport; the corrected probe uses the center ROI and its final log replaces that diagnostic run.

This covers visible `emission_curve` shader sampling and current-resource synchronization for GPUParticles3D on this host. Other curves and nodes, other rendering methods/platforms, and product ticket correlation remain open. No production source changed.
