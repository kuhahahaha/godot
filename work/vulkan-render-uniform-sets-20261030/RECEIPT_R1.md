# Vulkan render-uniform-set view validation — R28 — 2026-10-30

`command_bind_render_uniform_sets()` now rejects a positive set count when its uniform-set view is null or shorter than the requested count, before any set or shader dereference. The prior path crashed on an empty view. This is fail-closed native parameter validation; valid binding behavior and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one requested set with an empty view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 29 cases / 113 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 29/113, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.afa08c872`). Source diff check rc0.

Godot source HEAD at capture is `b6c03c5ff1a519b0d38e36dbe8fc1365982f0c79`; implementation commit is `b6c03c5ff1`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
