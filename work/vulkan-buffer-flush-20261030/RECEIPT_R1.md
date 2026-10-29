# Vulkan buffer-flush input validation — R53 — 2026-10-30

`buffer_flush()` now rejects an invalid `BufferID` before memory-property and allocation access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer flushing and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer flush crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 54 cases / 188 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 54/188 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.30bbf6051`). Source diff check rc0.

Godot source HEAD at capture is `84eb73dfdda9a7330a82e4a9a283851c531bc2f2`; implementation commit is `84eb73dfdd`. This is bounded native buffer-flush validation and does not close the separate owner or K03 operation/completion/readback joins.
