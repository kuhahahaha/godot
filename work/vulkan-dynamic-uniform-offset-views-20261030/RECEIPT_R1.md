# Vulkan dynamic-uniform-offset view validation — R41 — 2026-10-30

`uniform_sets_get_dynamic_offsets()` now rejects a nonzero requested set count when the supplied uniform-set view is null or shorter than the requested count, before uniform-set access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid offset queries and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null uniform-set view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 42 cases / 152 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 42/152 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.9df6bb18f`). Source diff check rc0.

Godot source HEAD at capture is `9b4a46cb0a300e779d31f55c8848a1b67c96d602`; implementation commit is `9b4a46cb0a`. This is bounded native uniform-set query validation and does not close the separate owner or K03 operation/completion/readback joins.
