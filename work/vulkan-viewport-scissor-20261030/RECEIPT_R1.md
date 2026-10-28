# Vulkan viewport/scissor view validation — R26 — 2026-10-30

`command_render_set_viewport()` and `command_render_set_scissor()` now reject nonempty `VectorView<Rect2i>` values with null backing pointers before command access. Both old paths crashed on the same malformed input. This is fail-closed native parameter validation; valid render-state commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: viewport and scissor null views each crashed with SIGSEGV, rc134.
- Debug GREEN: viewport 1 case / 3 assertions, scissor 1/3, Vulkan qualification 27 cases / 107 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 27/107 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.048652f50`). Source diff check rc0.

Godot source HEAD at capture is `40c84b0c7b095c92f4d6e7c477ccd576974cf5f0`; implementation commit is `40c84b0c7b`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
