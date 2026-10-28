# Vulkan texture-view swizzle validation — R19 — 2026-10-30

`RenderingDeviceDriverVulkan` now rejects every `TextureView` channel whose swizzle is outside `TEXTURE_SWIZZLE_MAX` before native image creation. The guard is applied to the original texture path and both shared-view paths. The change is a fail-closed input check; valid swizzles and all owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: the new test returned rc1 because all four invalid swizzles were accepted (`CHECK_FALSE(texture)` failed four times).
- Debug GREEN: focused Vulkan swizzle test 1 case / 7 assertions, rc0; Vulkan qualification regression 18 cases / 80 assertions, rc0; K03/P2 22 cases / 350 assertions, rc0.
- Actual `use_ubsan=yes`: focused 1/7 and Vulkan regression 18/80, both rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` both rc0 (`4.7.2.stable.custom_build.1e543b0f6`).
- Source diff check rc0.

Godot source HEAD at capture is `89e42a38244642dc6e259de7b5e557f5eb35cfae`. The implementation commit is `89e42a3824`. This package is bounded native input-safety evidence and does not claim the separate U01 owner or K03 operation/completion/readback joins.
