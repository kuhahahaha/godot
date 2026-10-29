# Vulkan buffer texel-format input validation — R54 — 2026-10-30

`buffer_set_texel_format()` now rejects an invalid `BufferID` before buffer-view creation. The malformed path previously crashed with SIGSEGV when given a valid format. This is fail-closed native resource-input validation; valid buffer view creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer with valid format crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 55 cases / 191 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 55/191 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.73856055d`). Source diff check rc0.

Godot source HEAD at capture is `011054f48bd688d7afdc089e13e08279ffe631fc`; implementation commit is `011054f48b`. This is bounded native buffer-texel-format validation and does not close the separate owner or K03 operation/completion/readback joins.
