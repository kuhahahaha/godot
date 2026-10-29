# Vulkan buffer device-address getter validation — R49 — 2026-10-30

`buffer_get_device_address()` now rejects an invalid `BufferID` before building the Vulkan address query. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid buffer address queries and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid buffer device-address query crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 50 cases / 176 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 50/176 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.adecb7474`). Source diff check rc0.

Godot source HEAD at capture is `f535314d682b3815fd8401d82bd8c727a7f9aa3b`; implementation commit is `f535314d68`. This is bounded native buffer-address getter validation and does not close the separate owner or K03 operation/completion/readback joins.
