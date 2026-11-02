# 2D GPU-to-CPU draw-order conversion receipt

Date: 2026-11-02. Base before this change: `37cb79c135`.

`GPUParticles2D` offers index, lifetime, and reverse-lifetime draw orders, while `CPUParticles2D` accepts index and lifetime. Direct enum casting passed reverse-lifetime value 2 into the CPU setter, which rejected it and left the target's previous order. Conversion now maps GPU index to CPU index and both lifetime variants to CPU lifetime.

The source fixture failed against the pre-fix HEAD and passed 1/1 against the worktree. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran the focused `*2D GPU reverse lifetime converts to CPU lifetime draw order*` doctest with 1 case and 1 assertion passed. Only this mapping line is staged from the dirty CPU 2D file. This is scene conversion state evidence under dummy rendering, not a GPU draw-order comparison.
