# Vulkan timestamp-query output validation — R45 — 2026-10-30

`timestamp_query_pool_get_results()` now rejects a nonzero query count with a null result-output pointer before calling Vulkan. The malformed path previously crashed with SIGSEGV. This is fail-closed native output validation; valid timestamp queries and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null result-output case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 46 cases / 164 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 46/164 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.7cd4bf172`). Source diff check rc0.

Godot source HEAD at capture is `b0f93aaac0d01fb297f2608717bbc43489a671f1`; implementation commit is `b0f93aaac0`. This is bounded native timestamp-output validation and does not close the separate owner or K03 operation/completion/readback joins.
