# GPU-to-CPU point texture conversion receipt

Date: 2026-11-02. Base before this change: `c00d40b8ad`.

The editor stores GPU point emission positions in RGF (2D) or RGBF (3D) ImageTextures, with optional matching normal textures and RGBA8 color textures. GPU-to-CPU conversion previously copied POINTS or DIRECTED_POINTS shape but left the CPU point arrays empty, so the CPU simulation rejected every spawn. Both conversion paths now decode the first `emission_point_count` texels when the readable image format and capacity match. Optional normals and colors are decoded when their own format and capacity match. Missing, unreadable, wrong-format or too-small images leave the corresponding CPU arrays empty; conversion does not retain stale arrays from a prior source.

The headless scene fixture failed before implementation for both dimensions (exit 1) and passed after implementation (exit 0), checking positions, normals, colors and point count beyond image capacity. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree. Rebuilt binary SHA256 `0ea75a6a083abc0f5ec87e177daf83c0033af831ebeb6cfc979e497694d32d91` also passed existing conversion doctests, 11 cases / 59 assertions. These checks verify scene data under dummy rendering, not live GPU emission or an isolated build of this commit.
