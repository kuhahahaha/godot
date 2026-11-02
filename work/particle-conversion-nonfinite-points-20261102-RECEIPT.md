# GPU-to-CPU nonfinite point texture receipt

Date: 2026-11-02. Base before this change: `46422401a3`.

GPU-to-CPU point texture conversion previously decoded all point coordinates, called the CPU point setter, then decoded normals and colors. The CPU setter rejects nonfinite positions, leaving points empty while optional normals and colors become populated. Both 2D and 3D conversions now validate the full point array before assigning any of the three arrays; a nonfinite point leaves all three empty. The normal present-material conversion path remains otherwise unchanged.

The headless fixture used INF in a 2D RGF point image and NaN in a 3D RGBF point image, with valid matching normal and color textures. It failed before implementation in both dimensions (exit 1), then passed after implementation (exit 0). The existing valid point-texture fixture also passed. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree; conversion doctests passed 11 cases / 59 assertions. Rebuilt binary SHA256 `c679326a3a287a5d52061585b80012f93eeb1e432a7556d4e7b56079246f9060`. These checks verify scene data under dummy rendering, not live GPU emission or an isolated build of this commit.
