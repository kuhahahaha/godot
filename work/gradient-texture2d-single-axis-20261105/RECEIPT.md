# GradientTexture2D single-axis receipt

`GradientTexture2D::_get_gradient_offset_at` now initializes the normalized
position to `(0,0)` before conditionally computing width/height divisions.
This makes one-pixel axes deterministic and avoids reading an indeterminate
component during gradient generation.

Verification:

- `scons tests=yes -j8 bin/godot.macos.editor.arm64`
- `bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gradient_texture.cpp' --no-colors`
- Result: 4 test cases, 20 assertions, all passed.

The focused test covers width=1, height=2 with a vertical linear gradient.
