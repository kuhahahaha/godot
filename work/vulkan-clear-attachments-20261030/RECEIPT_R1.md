# Vulkan clear-attachment view validation — R25 — 2026-10-30

`command_render_clear_attachments()` now rejects nonempty attachment-clear or rectangle views whose backing pointers are null before command-buffer access. The prior path dereferenced the null view and crashed. This is a fail-closed native parameter check; valid clear commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one null attachment-clear view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 25 cases / 101 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: focused 1/3 and Vulkan 25/101, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.506b27951`). Source diff check rc0.

Godot source HEAD at capture is `e339d2f2c9a08971056c284383c262b09a3bfa23`; implementation commit is `e339d2f2c9`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
