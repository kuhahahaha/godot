# Vulkan texture-free input validation — R55 — 2026-10-30

`texture_free()` now rejects an invalid `TextureID` before image-view destruction or allocation access. The malformed path previously crashed with SIGSEGV. This is fail-closed native resource-input validation; valid texture release and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: invalid texture free crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 56 cases / 194 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 56/194 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.9dafef84c`). Source diff check rc0.

Godot source HEAD at capture is `89d63b078ae5656086cb7da0a96a3222ba25efbd`; implementation commit is `89d63b078a`. This is bounded native texture-release validation and does not close the separate owner or K03 operation/completion/readback joins.
