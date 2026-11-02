# GPU-to-CPU conversion resource reset receipt

Date: 2026-11-02. Base before this change: `ec2ecab8fa`.

Repeated GPU→CPU conversion in 2D and 3D now assigns the current parameter curve and both color ramps even when the source material's texture reference is empty. That clears a reused CPU node's old resource references after the source removes them. The staged source is limited to the two ramp replacements and parameter-curve macro replacement in each CPU file; other dirty conversion work remains unstaged.

The source fixture failed against the pre-fix HEAD and passed 6/6 on the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `*repeated GPU to CPU conversion clears removed curves and ramps*` with 1 case and 12 assertions passed across both dimensions. This is dummy-renderer scene-state evidence, not GPU output or an isolated build. A null GPU process material still exits before per-material conversion and is outside this commit's contract.
