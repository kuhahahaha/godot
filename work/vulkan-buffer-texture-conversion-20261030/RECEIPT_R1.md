# Vulkan buffer↔texture conversion region validation — R34/R35 — 2026-10-30

`command_copy_buffer_to_texture()` and `command_copy_texture_to_buffer()` now reject nonempty region views with null backing pointers before texture/buffer access. Both old paths crashed on malformed views. This is fail-closed native input validation; valid conversion commands and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: buffer→texture and texture→buffer null region views each crashed with SIGSEGV, rc134.
- Current Debug: focused buffer→texture and texture→buffer each pass 1 case / 3 assertions; Vulkan qualification regression is 36 cases / 134 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 36/134 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.23573e926`). Source diff check rc0.

Godot source HEAD at capture is `614e4ee8f400304a2c9a2b5b8fb919a3623f764b`; implementation commit is `614e4ee8f4`. This is bounded native transfer-input validation and does not close the separate owner or K03 operation/completion/readback joins.
