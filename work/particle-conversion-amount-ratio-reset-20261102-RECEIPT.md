# CPU-to-GPU amount ratio reset receipt

Date: 2026-11-02. Base before this change: `c42a762ba9`.

CPUParticles2D and CPUParticles3D have no `amount_ratio`. A reused GPU target can retain an earlier ratio below one after conversion, so copying the CPU particle count alone does not preserve its emission count. Both conversion paths now reset the target ratio to 1 immediately after copying the amount.

The source fixture failed against the pre-fix HEAD and passed 2/2 on the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `*CPU to GPU conversion resets target amount ratio*` with 1 case and 2 assertions passed. This checks scene state under dummy rendering; it does not establish live GPU emission or an isolated build of this commit.
