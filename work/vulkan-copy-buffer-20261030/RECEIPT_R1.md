# Vulkan buffer-copy region validation — R32 — 2026-10-30

`command_copy_buffer()` now rejects nonempty copy-region views with null backing pointers before command or buffer access. The prior path crashed on an empty region view. This is fail-closed native input validation; valid buffer copies and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: one null region view crashed with SIGSEGV, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 33 cases / 125 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 33/125, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.93cf238ed`). Source diff check rc0.

Godot source HEAD at capture is `de1e2c929240751e382287fbb3609b5b7049d02e`; implementation commit is `de1e2c9292`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
