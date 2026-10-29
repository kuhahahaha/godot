# Vulkan texture allocation-size getter validation — R47 — 2026-10-30

`texture_get_allocation_size()` now rejects an invalid `TextureID` before dereferencing `TextureInfo`. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid texture getters and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid texture allocation-size query crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 48 cases / 170 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 48/170 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.6a4d634de`). Source diff check rc0.

Godot source HEAD at capture is `64ead84dd97a8a1cafb7f90f38877a8b9139fc84`; implementation commit is `64ead84dd9`. This is bounded native texture-getter validation and does not close the separate owner or K03 operation/completion/readback joins.
