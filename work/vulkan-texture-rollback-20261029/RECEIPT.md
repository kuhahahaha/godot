# Vulkan texture allocation rollback — 2026-10-29

This D/native slice repairs a real partial-allocation leak in
`RenderingDeviceDriverVulkan::texture_create` when extra GPU memory tracking
uses the manual `vkCreateImage` → `vmaAllocateMemoryForImage` →
`vmaBindImageMemory2` path. Allocation and bind failures now destroy the
original image and free any acquired VMA allocation before returning. Image
view failure uses the same rollback path.

## TDD and validation

- RED runtime test rc1 reproduced the missing rollback: both injected stages
  returned a non-null texture, failed the rollback-mask assertions, increased
  VMA allocation count, and terminated with VMA's live-allocation assertion.
- GREEN Debug focused test: 1 case / 8 assertions, rc0.
- Existing Vulkan allocation regression: 2 cases / 16 assertions, rc0.
- K03/P2 regression: 22 cases / 350 assertions, rc0.
- macOS arm64 `template_release` build and `--version`: rc0.
- Actual `use_ubsan=yes` build and focused test: rc0, 1 case / 8 assertions.

The rollback mask proves stage 1 releases the image and stage 2 releases both
image and memory; VMA allocation count returns to its pre-call value. No
fallback texture, synthetic owner, qualification authority, or GPU/Ready claim
is introduced. This is native allocation cleanup evidence only.
