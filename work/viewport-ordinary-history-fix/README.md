# Ordinary viewport camera-history fix

Ordinary viewports have no presentation token. The scene cull path now calls the existing `viewport_set_prev_camera_data()` path directly for those viewports; only registered presentation keys use strict token staging. This preserves two-frame history while keeping registered foreign-token rejection.

The change is applied in the mixed working tree and intentionally not committed as a standalone production commit because renderer_viewport.cpp and renderer_scene_cull.cpp contain the pre-existing C01/E15 migration owned by another stream.

Build (exit 0): `scons platform=macos target=editor dev_build=yes tests=yes bin/godot.macos.editor.dev.arm64 -j2`

Regression (exit 0; 1 case, 24 assertions):
`./bin/godot.macos.editor.dev.arm64 --test --test-case=\'[ViewportHistory] ordinary two-frame history preserves registered admission\'`

Visible native scene (exit 0; Vulkan Apple M3 Pro):
`./bin/godot.macos.editor.dev.arm64 --display-driver macos --rendering-method mobile --rendering-driver vulkan --audio-driver Dummy --path /Users/macbookair/Documents/Work/ANGE/work/particle-integration-baseline-20261025 --script native_particle.gd`
Output: `PARTICLE_BASELINE_OK moved=true visible=true finished=true freed=true`; no camera ordinal error.

`production.patch` freezes both production corrections (including the earlier ordinal correction), relative to the exact pre-fix dirty source hashes in `identity.json`. Apply only to that owning migration baseline; it is already applied in this working tree. The test exercises the two public history routes; the visible fixture exercises the real scene-cull caller.
