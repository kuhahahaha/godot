# Vulkan texture-data getter validation — R56 — 2026-10-30

`texture_get_data()` now rejects an invalid `TextureID` before texture state access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid texture readback and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid texture data query crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 57 cases / 197 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 57/197 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.785afaefd`). Source diff check rc0.

Godot source HEAD at capture is `b325d75293637eb3c151e531c1b40dd220e797e6`; implementation commit is `b325d75293`. This is bounded native texture-data getter validation and does not close the separate owner or K03 operation/completion/readback joins.
