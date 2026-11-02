# GPU-to-CPU point shape transform receipt

Date: 2026-11-02. Base before this change: `5d3b60b778`.

ParticleProcessMaterial applies `emission_shape_scale` and `emission_shape_offset` to emitted positions after sampling its point texture. GPU-to-CPU conversion previously decoded only the raw texel position, so the CPU target emitted at a different location. The 2D and 3D paths now apply the corresponding scale and offset to each decoded point before validating and storing it. The post-transform finite check also rejects overflow from otherwise finite texels.

The headless fixture failed before implementation for both dimensions (exit 1) and passed after implementation (exit 0), asserting hand-calculated transformed positions. The existing valid point-texture fixture passed. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree; conversion doctests passed 11 cases / 59 assertions. Rebuilt binary SHA256 `609ea7d62edbf1d151a14494e0765529f9c8b909a33c4f51c9d11f76268fba94`. These checks verify scene data under dummy rendering, not live GPU emission or an isolated build of this commit.
