# Vulkan framebuffer attachment-view validation — R38 — 2026-10-30

`framebuffer_create()` now rejects a nonempty attachment view with a null backing pointer before texture/resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid framebuffer creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null attachment view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 39 cases / 143 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 39/143 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.acc403ae2`). Source diff check rc0.

Godot source HEAD at capture is `39e7090078755d2bb0da1965c44dc6f49161f134`; implementation commit is `39e7090078`. This is bounded native framebuffer-input validation and does not close the separate owner or K03 operation/completion/readback joins.
