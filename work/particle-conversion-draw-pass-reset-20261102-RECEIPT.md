# CPU-to-GPU 3D draw-pass reset receipt

Date: 2026-11-02. Base before this change: `7e8670bb81`.

CPUParticles3D has one mesh, while a reused GPUParticles3D target may have multiple draw passes. Conversion now reduces the target to one pass before assigning the source mesh, so removed passes and their mesh references do not persist. Only the single conversion line is staged from the otherwise dirty GPU 3D file.

The source fixture failed against the pre-fix HEAD and passed 2/2 on the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `*repeated CPU to GPU 3D conversion removes extra draw passes*` with 1 case and 2 assertions passed. This checks scene state under dummy rendering; it does not establish live GPU draw behavior or an isolated build of this commit.
