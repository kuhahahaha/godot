# GPU-to-CPU split-scale reset receipt

Date: 2026-11-02. Base before this change: `9956f232e0`.

When a repeated GPU→CPU conversion removes a `CurveXYZTexture` from an otherwise present process material, the 2D/3D CPU target now derives `split_scale` from current texture presence and clears old X/Y/Z curve references before copying any current curves. Previously the old mode and curves remained active. Only these conversion hunks are staged; a wholly null GPU process material still returns before per-material state replacement and needs a separate full-state decision.

The source fixture failed against the pre-fix HEAD and passed 7/7 on the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `*repeated GPU to CPU conversion clears obsolete split scale*` with 1 case and 8 assertions passed. This verifies scene state under dummy rendering, not GPU output or an isolated build.
