# Vulkan shared-texture slice owner regression — R59 — 2026-10-30

R59 adds focused coverage for the owner guard already implemented by R58 in `texture_create_shared_from_slice()`. The invalid original `TextureID` slice path now returns an empty `TextureID` without resource access.

Evidence:

- Current Debug focused: 1 case / 3 assertions, rc0.
- Vulkan qualification regression: 60 cases / 206 assertions.
- K03/P2: 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 60/206 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.d33785e79`). Source diff check rc0.

Godot source HEAD at capture is `08d9fb3168877547fd7bf1910acbc0606ef9e778`; test commit is `08d9fb3168`. This test supplements R58's bounded shared-texture owner validation and does not claim ANGE owner, completion, readback, Ready or G1 authority.
