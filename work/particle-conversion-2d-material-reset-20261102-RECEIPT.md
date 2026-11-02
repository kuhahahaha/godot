# Repeated 2D particle conversion material reset receipt

Date: 2026-11-02. Base before this change: `4c649bdb34`.

CPU↔GPU 2D conversion now applies the source CanvasItem `Material` even when its reference is empty. Previously both conversion paths only set a valid material; reusing a target after removing the source material retained the target's old render material. Only the two conditional-to-unconditional conversion hunks are staged from the otherwise dirty files.

`python3 tests/scene/fixtures/particle_conversion_2d_material_reset/run.py --source-ref HEAD` failed on the pre-fix CPU path; the worktree passed 2/2 source checks. The mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran the focused `*repeated 2D conversion clears removed render material*` doctest with 1 case and 4 assertions passed, covering both directions. This verifies scene state under dummy rendering, not GPU output or an isolated build.
