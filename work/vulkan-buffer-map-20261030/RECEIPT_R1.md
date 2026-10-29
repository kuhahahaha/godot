# Vulkan buffer-map input validation — R50 — 2026-10-30

`buffer_map()` now rejects an invalid `BufferID` before dynamic-buffer/resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer mapping and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer map crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 51 cases / 179 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 51/179 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.5935ecb3d`). Source diff check rc0.

Godot source HEAD at capture is `deb180247ea212cbe79a5188e604e29b771402f5`; implementation commit is `deb180247e`. This is bounded native buffer-map validation and does not close the separate owner or K03 operation/completion/readback joins.
