# GPUParticles3D initial-color ramp visible resource synchronization

Date: 2026-11-04. Fixed editor SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. Probe SHA-256 `bb8d7f57e4ed1d53ef1d56f218d6644369c2fb53bfbf2581e1e09a2909f46d1f`.

A fixed-seed GPUParticles3D node drew eight white unshaded quads using `ParticleProcessMaterial.color_initial_ramp`. The retained `GradientTexture1D` began red, changed to green in place, was replaced by blue, and then the detached old gradient changed to yellow. Visible Metal Mobile and Vulkan Mobile both produced exact `(red, green, blue)` counts `256,0,0 → 0,256,0 → 0,0,256 → 0,0,256`; both processes exited 0 and saved each capture. No engine or script error appeared. Replacement and detached-old PNGs have identical SHA-256 `384df9687ea9b408f29cafb07a54284836b7dff2f516f23b23d1847fd42b4cc6`.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-initial-ramp-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw per-driver logs are retained beside the probe. The PNGs are from the final Vulkan run because the output directory was reused; the Metal observations remain in its log.

This covers the visible initial-color ramp shader path, same-resource mutation, replacement, and detached-old isolation for GPUParticles3D on this host. Emission and velocity-limit curves, other particle nodes and methods, and product ticket correlation remain open. No production source changed.
