# GPUParticles3D alpha-curve visible resource synchronization

Date: 2026-11-04. The fixed editor SHA-256 is `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; its controlled-build sidecar SHA-256 is `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`. The retained probe SHA-256 is `f3f36776fff071fd42fc7a150a353b844e86cd99af7f8bdc0fca75a44d889603`.

A white unshaded GPUParticles3D quad used a `GradientTexture1D` in `ParticleProcessMaterial.alpha_curve`, with alpha transparency enabled on its draw material. In both visible Metal Mobile and Vulkan Mobile, changing the same gradient from opaque to transparent, replacing the texture with an opaque one, and finally changing the detached old gradient to 0.25 produced exactly `256 → 0 → 256 → 256` white pixels. Both processes exited 0, all PNG captures saved, and neither log contains an engine or script error. The replacement and detached-old PNG SHA-256 match at `c1c4f4936f402fb62c5d8a5ddb8c9a9c4a667b83aac6ca129b52842873c43353`.

Run form: `bin/godot.macos.editor.arm64 --path work/particle-gpu3d-alpha-curve-visible-20261105 --rendering-driver <metal|vulkan> --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs and PNGs are beside this receipt. The same output directory is reused across renderers; the PNGs correspond to the final Vulkan run, while the Metal observations are retained in its log.

This verifies the visible alpha-curve shader path and current-resource synchronization for GPUParticles3D on this host. It does not cover other curve slots, other particle nodes or methods, or product ticket correlation. No production source changed.
