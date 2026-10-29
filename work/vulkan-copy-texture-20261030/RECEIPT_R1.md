# Vulkan texture-copy region validation — R33 — 2026-10-30

`command_copy_texture()` now rejects nonempty texture-copy region views with null backing pointers before layout/resource access. The prior path crashed on an empty region view. This is fail-closed native input validation; valid texture copies and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one null region view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 34 cases / 128 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 34/128, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.23573e926`). Source diff check rc0.

Godot source HEAD at capture is `1a251fca8eb55437f9952481a036bb996768a745`; implementation commit is `1a251fca8e`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
