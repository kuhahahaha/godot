# Vulkan buffer allocation-size getter validation — R48 — 2026-10-30

`buffer_get_allocation_size()` now rejects an invalid `BufferID` before dereferencing `BufferInfo`. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer getters and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer allocation-size query crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 49 cases / 173 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 49/173 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.a40a8f7fd`). Source diff check rc0.

Godot source HEAD at capture is `3106d65d5f94b8eccac9c9400773c5c4f5e66180`; implementation commit is `3106d65d5f`. This is bounded native buffer-getter validation and does not close the separate owner or K03 operation/completion/readback joins.
