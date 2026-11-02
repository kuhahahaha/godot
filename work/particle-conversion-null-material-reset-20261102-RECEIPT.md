# GPU-to-CPU null process material reset receipt

Date: 2026-11-02. Base before this change: `86a7a80551`.

Converting a GPU particle node with no ParticleProcessMaterial into a reused CPU node previously copied common node settings, then returned while retaining the CPU node's prior processing state. Both 2D and 3D conversion paths now restore CPU constructor defaults for direction, spread, color, ramps, flags, emission geometry, split scale, gravity, lifetime randomness, and all parameter ranges and curves. CPU emission point, normal and color arrays are cleared on both null and present material paths because these arrays have no copied source data in the conversion. Normal present-material conversion remains unchanged otherwise.

The headless scene fixture compares a fresh and a deliberately mutated reused CPU target after conversion in both dimensions. It failed before the fix with stale direction, spread, color, shape, gravity, curve, parameter and flag reports (exit 1). `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree. Rebuilt binary SHA256 `32df190d3de48b1ae6b9e0720134be3751da8c36830f355f50d94e379d30fb86` ran the fixture successfully (exit 0), including present-material stale emission-point checks. This verifies scene state under dummy rendering, not live GPU particle output or an isolated build of this commit.
