# Vulkan render-pass clear-view validation — R36 — 2026-10-30

`command_begin_render_pass()` now rejects a nonempty clear-value view with a null backing pointer before command, render-pass, or framebuffer access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid render-pass commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null clear-value view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 37 cases / 137 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 37/137 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.dedd76135`). Source diff check rc0.

Godot source HEAD at capture is `343c7cf77536dc64fd0811a8e0f6c9a4949b117b`; implementation commit is `343c7cf775`. This is bounded native command-input validation and does not close the separate owner or K03 operation/completion/readback joins.
