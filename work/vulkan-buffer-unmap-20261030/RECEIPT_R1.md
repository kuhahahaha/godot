# Vulkan buffer-unmap input validation — R51 — 2026-10-30

`buffer_unmap()` now rejects an invalid `BufferID` before unmapping allocation memory. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer unmapping and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer unmap crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 52 cases / 182 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 52/182 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.17ddfb881`). Source diff check rc0.

Godot source HEAD at capture is `99d7961a148e610f4acee67d21c1d1ea6a07d493`; implementation commit is `99d7961a14`. This is bounded native buffer-unmap validation and does not close the separate owner or K03 operation/completion/readback joins.
