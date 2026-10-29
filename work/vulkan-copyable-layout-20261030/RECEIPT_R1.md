# Vulkan copyable-layout output validation — R46 — 2026-10-30

`texture_get_copyable_layout()` now rejects a null output pointer before texture/resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native output validation; valid layout queries and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null copyable-layout output case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 47 cases / 167 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual current-source `use_ubsan=yes` build retry R3 completed rc0; Vulkan qualification is 47 cases / 167 assertions with no sanitizer diagnostics. The earlier mixed-worktree interface race and both failed attempts remain in `UBSAN_BUILD_R1/R2.log` and `BUILD_BLOCKER.md` as audit history.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.d3c10b4b4`). Source diff check rc0.

Godot source HEAD at capture is `70daa5ee966120df071f62ae9c6877afb9ee6c7d`; implementation commit is `70daa5ee96`. This is bounded native copyable-layout output validation and does not close the separate owner or K03 operation/completion/readback joins.
