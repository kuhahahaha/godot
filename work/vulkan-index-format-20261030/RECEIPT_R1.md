# Vulkan index-buffer format validation — R23 — 2026-10-30

`command_render_bind_index_buffer()` now rejects malformed `IndexBufferFormat` values before dereferencing the command or buffer handles. The guard reads enum storage bytes and accepts only UINT16/UINT32, preventing the prior invalid-format/null-command SIGSEGV. Valid bindings and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: invalid format with null command/buffer crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 23 cases / 95 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: focused 1/3 and Vulkan 23/95, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.9ccdd6dd3`). Source diff check rc0.

Godot source HEAD at capture is `aaaa29c3c1cbe965ec11bc8caef6fc9aa1c4ecb1`; implementation commit is `aaaa29c3c1`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
