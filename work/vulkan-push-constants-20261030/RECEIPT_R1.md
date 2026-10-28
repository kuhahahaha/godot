# Vulkan push-constant data validation — R27 — 2026-10-30

`command_bind_push_constants()` now rejects a nonempty `VectorView<uint32_t>` with a null data pointer before command/shader access. The prior path dereferenced malformed input and crashed. This is fail-closed native parameter validation; valid push-constant commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one null data element crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 28 cases / 110 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 28/110, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.c870afe70`). Source diff check rc0.

Godot source HEAD at capture is `1d8b6270f9b72b701418366ed0e58d922f35c3e9`; implementation commit is `1d8b6270f9`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
