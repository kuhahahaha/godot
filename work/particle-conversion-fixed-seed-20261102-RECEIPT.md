# Particle conversion fixed-seed receipt

Date: 2026-11-02. Base before this change: `3384754b4a`.

CPU↔GPU conversion now copies `use_fixed_seed` and `seed` in both 2D and 3D before applying the source emitting state. Without the copy, converted emitters can lose deterministic seed state or keep a stale seed on a reused target. The staged source contains only the two assignment lines in each of the four conversion functions; other dirty conversion changes remain unstaged.

The source fixture failed against the pre-fix HEAD and passed 4/4 on the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran `./bin/godot.macos.editor.arm64 --headless --test --test-case='*CPU and GPU conversion preserves fixed seed in 2D and 3D'` with 1 case and 10 assertions passed, covering both directions/dimensions and disabling fixed seed on reuse. This checks scene conversion state under dummy rendering; it does not establish GPU output or an isolated build of this commit.
