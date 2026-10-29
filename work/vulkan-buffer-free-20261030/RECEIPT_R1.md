# Vulkan buffer-free input validation — R57 — 2026-10-30

`buffer_free()` now rejects an invalid `BufferID` before buffer-view, mapping, or allocation access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer release and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer free crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 58 cases / 200 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 58/200 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.31cb3c74a`). Source diff check rc0.

Godot source HEAD at capture is `29ca024c8685aea72ee58c8d5412ab036c6b795b`; implementation commit is `29ca024c86`. This is bounded native buffer-release validation and does not close the separate owner or K03 operation/completion/readback joins.
