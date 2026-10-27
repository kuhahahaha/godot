# K03 current-pair ABI drift negative receipt — 2026-10-27

- Fresh VFX and no-VFX probes both stop at extension initialization.
- First error: missing `device_exit_register_scene_ready_v2` in the editor.
- VFX/no-VFX: rc2, `U01_EXTENSION_STATUS=1`, `U01_EXTENSION_OK=false`.
- No world/VFX/close/completion/readback/Ready/GPU behavior was reached.
- The DSO was not weakened or retargeted; the next required input is a matching editor host.
