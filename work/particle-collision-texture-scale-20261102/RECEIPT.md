# GPU 2D particle collision size after texture changes

Date: 2026-11-02

`GPUParticles2D::_update_collision_size()` computes the effective collision radius in float representation and sends zero if a mutable texture change would make it non-finite. `set_collision_base_size()` rejects a scalar whose current texture dimensions would overflow that representation before mutating node state. `ImageTexture::set_size_override()` emits `changed`, allowing the existing GPUParticles2D texture connection to recompute the renderer value. Texture replacement's pre-commit scaling guard was already committed before this change.

The source fixture was RED at pre-change HEAD (3/8) and GREEN on current source (8/8). `scons tests=yes platform=macos target=editor -j4` completed rc 0 on the mixed tree; resulting binary SHA256 `9e3664c7897df1e67f91be8e745c990d3841fbfff4e325d1139feb200a8173a2`. Focused native cases passed: GPU 2D collision scaling 1 case / 4 assertions; ImageTexture size override notification 1 case / 1 assertion. Staged diff check passed.

The native tests validate node state and signal emission. They do not read back the backend collision radius after a mutable texture resize or qualify a GPU collision frame. The isolated patch includes two GPU 2D functions, ImageTexture notification, the two focused tests, fixture and this receipt; other dirty bounds and test changes remain unstaged.
