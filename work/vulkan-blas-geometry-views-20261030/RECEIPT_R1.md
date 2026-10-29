# Vulkan BLAS geometry-view validation — R44 — 2026-10-30

`blas_create()` now rejects a nonempty geometry view with a null backing pointer before geometry validation or ray-tracing feature access. The malformed path previously crashed with SIGSEGV even when ray tracing was disabled. This is fail-closed native input validation; valid BLAS creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null geometry view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 45 cases / 161 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 45/161 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.438b4c0fc`). Source diff check rc0.

Godot source HEAD at capture is `72b3ede34218be8ffdc1464167ef41cb8521a21b`; implementation commit is `72b3ede342`. This is bounded native BLAS-input validation and does not close the separate owner or K03 operation/completion/readback joins.
