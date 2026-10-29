# Vulkan persistent buffer-map input validation — R52 — 2026-10-30

`buffer_persistent_map_advance()` now rejects an invalid `BufferID` before dynamic-buffer state access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid persistent mapping and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid persistent buffer-map advance crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 53 cases / 185 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 53/185 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.a8f1e2c1c`). Source diff check rc0.

Godot source HEAD at capture is `425221b06d0197abfd369a7d39033b6296e5485c`; implementation commit is `425221b06d`. This is bounded native persistent-map validation and does not close the separate owner or K03 operation/completion/readback joins.
