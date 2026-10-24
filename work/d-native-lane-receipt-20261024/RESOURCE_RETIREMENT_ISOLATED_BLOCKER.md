# Isolated resource-retirement patch blocker — 2026-10-24

A detached clean worktree at `f323670334` was created to isolate the saved four-hunk `resource-retirement-error-propagation.patch`. The patch does not apply to that clean HEAD because the expected `capture_test_resource_registry.observe_free(...)` call sites are introduced by the broader uncommitted rendering-device/test-registry migration, not by HEAD itself.

Therefore the patch cannot be independently committed without first adopting that migration. The mixed main tree also contains unrelated dirty changes in `rendering_device.cpp` and renderer/resource files. No production source was changed in the isolated worktree.

The intended patch remains narrowly defined: propagate duplicate/stale deferred-free observations into `capture_test_coverage_failed` for compute pipelines, uniform sets, shaders, and textures. Full build is still blocked by `No space left on device`; runtime/native/GPU/PCK/production qualification remains false.
