# Vulkan compute-uniform-set view validation — R29 — 2026-10-30

`command_bind_compute_uniform_sets()` now rejects positive set counts with null or undersized uniform-set views before dereference. The prior path crashed on an empty view. This is fail-closed native input validation; valid compute bindings and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one requested set with an empty view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 30 cases / 116 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 30/116, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.b1fa1d793`). Source diff check rc0.

Godot source HEAD at capture is `509c30a82783a7667eda9ed9f27ab557dfd056ec`; implementation commit is `509c30a827`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
