# Vulkan pipeline-barrier view validation — R37 — 2026-10-30

`command_pipeline_barrier()` now rejects any nonempty barrier view with a null backing pointer before barrier conversion or command-buffer access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid barrier commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null memory-barrier view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 38 cases / 140 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 38/140 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.999c97c24`). Source diff check rc0.

Godot source HEAD at capture is `041cce253cded886ad1ec20660b7e560181d65e3`; implementation commit is `041cce253c`. This is bounded native barrier-input validation and does not close the separate owner or K03 operation/completion/readback joins.
