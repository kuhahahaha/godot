# Vulkan render-pipeline view validation — R42 — 2026-10-30

`render_pipeline_create()` now rejects nonempty color-attachment or specialization-constant views with null backing pointers before pipeline/resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid pipeline creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null color-attachment view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 43 cases / 155 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 43/155 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.01db86a00`). Source diff check rc0.

Godot source HEAD at capture is `57dcb9d4a056c70c4b5a2e1b6ae822a176343c85`; implementation commit is `57dcb9d4a0`. This is bounded native pipeline-input validation and does not close the separate owner or K03 operation/completion/readback joins.
