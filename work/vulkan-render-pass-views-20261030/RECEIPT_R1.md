# Vulkan render-pass top-level view validation — R40 — 2026-10-30

`render_pass_create()` now rejects any nonempty attachment, subpass, or dependency view with a null backing pointer before validation or Vulkan resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid render-pass creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null attachment view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 41 cases / 149 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 41/149 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.ce7e517b3`). Source diff check rc0.

Godot source HEAD at capture is `e5b9a07b0c0bfb6d19d4d34fc89b042fe5a4ea30`; implementation commit is `e5b9a07b0c`. This is bounded native render-pass-input validation and does not close the separate owner or K03 operation/completion/readback joins.
