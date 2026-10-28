# Vulkan vertex-binding array validation — R24 — 2026-10-30

`command_render_bind_vertex_buffers()` now rejects a nonzero binding count with missing buffer or offset arrays before touching command-buffer state. The prior path dereferenced null inputs and crashed. This is a fail-closed native parameter check; valid bindings and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one binding with null arrays crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 24 cases / 98 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: focused 1/3 and Vulkan 24/98, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.9420e6dbc`). Source diff check rc0.

Godot source HEAD at capture is `67a4c2eea3e22b19621f7a9781b69c10016b02e3`; implementation commit is `67a4c2eea3`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
