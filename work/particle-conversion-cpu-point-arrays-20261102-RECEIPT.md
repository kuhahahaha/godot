# CPU-to-GPU point array conversion receipt

Date: 2026-11-02. Base before this change: `07182616bc`.

CPU particle POINTS and DIRECTED_POINTS use position, normal and color arrays, while ParticleProcessMaterial uses point textures. CPU-to-GPU conversion previously copied only the shape, leaving the new GPU material without its required point texture. Both dimensions now pack nonempty CPU point arrays into the editor's RGF (2D) or RGBF (3D) image format, set the exact point count, and pack matching normals and colors into RGF/RGBF and RGBA8 textures. A new process material is created per conversion, so absent or mismatched optional arrays leave no stale texture.

The headless scene fixture failed before implementation for both dimensions (exit 1). It passed after implementation (exit 0), checking generated textures and exact CPU→GPU→CPU point, normal and color data. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree. Rebuilt binary SHA256 `5d46a64db224b71cb5bb24b6c620a717bdae3028a75fbb834003fc537ddeeb3e` passed existing conversion doctests, 11 cases / 59 assertions. These checks verify scene data under dummy rendering, not live GPU emission or an isolated build of this commit.
