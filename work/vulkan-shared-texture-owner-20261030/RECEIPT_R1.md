# Vulkan shared-texture owner validation — R58 — 2026-10-30

`texture_create_shared()` and `texture_create_shared_from_slice()` now reject an invalid original `TextureID` before owner texture/view access. The malformed shared-texture path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid shared texture creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid original texture owner crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 59 cases / 203 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 59/203 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.fae5bff0d`). Source diff check rc0.

Godot source HEAD at capture is `9f0e1f34b87ff1ee9800cd591f40ecaba09f170b`; implementation commit is `9f0e1f34b8`. This is bounded native shared-texture owner validation and does not close the separate owner or K03 operation/completion/readback joins.
