# Vulkan secondary-command view validation — R30 — 2026-10-30

`command_buffer_execute_secondary()` now rejects a nonempty secondary-command `VectorView` with a null backing pointer before iteration or command access. The prior path crashed on an empty view. This is fail-closed native input validation; valid secondary command execution and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one null secondary-command view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 31 cases / 119 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 31/119, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.1efc19a61`). Source diff check rc0.

Godot source HEAD at capture is `6bdb411c9baba68f1b862fb272c76f6511b42faf`; implementation commit is `6bdb411c9b`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
