# Vulkan sampler-repeat validation — R21 — 2026-10-30

`RenderingDeviceDriverVulkan::sampler_create()` now rejects `SAMPLER_REPEAT_MODE_MAX` for U/V/W before native sampler creation. The prior code cast the sentinel directly to `VkSamplerAddressMode` and accepted it on the current backend. This is a fail-closed native input check; valid sampler behavior and all owner/operation/completion/readback/Ready/G1 semantics remain unchanged.

Evidence:

- TDD RED against the pre-fix binary: invalid `repeat_u` was accepted, focused test rc1 with one failed assertion.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification 20 cases / 86 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: focused 1/3 and Vulkan 20/86, both rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.c740a4f73`). Source diff check rc0.

Godot source HEAD at capture is `376294b26291bade55398cb56269af56f2fc375f`; implementation commit is `376294b262`. This package is bounded native input-safety evidence and does not close the separate owner/operation/completion/readback or Ready/G1 seams.
