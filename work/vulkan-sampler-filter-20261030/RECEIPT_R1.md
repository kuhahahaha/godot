# Vulkan sampler-filter validation — R20 — 2026-10-30

`RenderingDeviceDriverVulkan::sampler_create()` now rejects invalid values for all three `SamplerFilter` fields instead of silently mapping them to NEAREST. The fail-closed check reads the enum storage bytes before validation so malformed native input is rejected without triggering UBSan's invalid-enum load. Valid sampler behavior is unchanged.

Evidence:

- TDD RED against the pre-fix binary: `mag_filter=2` was silently accepted; the focused test returned rc1 with one failed assertion.
- An intermediate UBSan run proved that direct enum comparison itself emitted `runtime error: load of value 2, which is not a valid value for type 'SamplerFilter'` despite the functional test passing. That diagnostic is retained as `UBSAN_PRE_FIX*` evidence and is not counted as green.
- Final Debug: focused 1 case / 3 assertions, Vulkan qualification 19 cases / 83 assertions, and K03/P2 22 cases / 350 assertions, all rc0.
- Final actual `use_ubsan=yes`: focused 1/3 and Vulkan 19/83, rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.b9acf9a03`). Source diff check rc0.

Godot source HEAD at capture is `886caff847504ca0c95e2b91c6cbbbcf34dfdd6e`; implementation commit is `886caff847`. This is bounded native input validation and does not add owner, operation, completion, readback, Ready or G1 authority.
