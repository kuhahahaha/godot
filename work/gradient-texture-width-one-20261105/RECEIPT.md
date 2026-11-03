# GradientTexture1D width-one guard — 2026-11-05

`GradientTexture1D` generated texture data and the native resource snapshot path divided by `width - 1`. Width 1 now samples offset 0 explicitly in both paths, avoiding non-finite offsets while preserving the endpoint color.

Added a SceneTree regression covering a width-one texture and endpoint color. Fixed editor build: `scons tests=yes -j8 bin/godot.macos.editor.arm64` (rc 0). Focused fixed-editor run:

`bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gradient_texture.cpp,*test_resource_snapshot_bridge.cpp' --no-colors`

Result: 17 test cases, 198 assertions, all passed; 2034 skipped. This is a bounded resource-generation/snapshot fix, not GPU particle or product six-link evidence.
