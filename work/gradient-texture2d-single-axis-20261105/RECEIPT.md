# GradientTexture2D single-axis receipt

`GradientTexture2D::_get_gradient_offset_at` initializes the normalized
position to `(0,0)` before conditionally computing width/height divisions.
Both one-pixel-axis cases are covered: width=1/height=2 vertical sampling
and width=2/height=1 horizontal sampling.

Verification:

- `scons tests=yes -j8 bin/godot.macos.editor.arm64`
- `bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gradient_texture.cpp' --no-colors`
- Result: 4 test cases, 26 assertions, all passed.
