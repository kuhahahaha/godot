# Vulkan compute-pipeline specialization-view validation — R43 — 2026-10-30

`compute_pipeline_create()` now rejects a nonempty specialization-constant view with a null backing pointer before shader/pipeline resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid compute pipeline creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null specialization view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 44 cases / 158 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 44/158 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.5044b1aa9`). Source diff check rc0.

Godot source HEAD at capture is `96c8b1ed0e37441f9ccaf561d27b7e6860b6a500`; implementation commit is `96c8b1ed0e`. This is bounded native compute-pipeline-input validation and does not close the separate owner or K03 operation/completion/readback joins.
