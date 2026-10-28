# E06 macOS external glslang route — R1 receipt

Date: 2026-10-29

This receipt records one bounded native continuation of the E06 queue. The
macOS Vulkan build previously stopped before compilation because the external
glslang route did not provide the `glslang/Public` include root. The surgical
change in `platform/macos/detect.py` now searches an explicitly selected
Vulkan SDK root, then the two standard Homebrew prefixes, and requires both
`include/glslang/Public/ShaderLang.h` and a `lib` directory before appending
the three external libraries. If no complete installation exists, SCons exits
255; no builtin fallback or fabricated identity is introduced.

## Build evidence

- Godot HEAD: `3e5f2cabe6e2808bc68e28f2499c0be6b0a4fbb6`
- Source change: `platform/macos/detect.py`, SHA256
  `3415360e36a9182e8526ccb7d28001450f20066ad8e9981fc993ea59fff2be88`
- Selected external installation: `/opt/homebrew/opt/glslang` → glslang 16.6.0
- Command: `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=no builtin_glslang=no`
- Build return code: `0` (`BUILD_R1.rc`)
- Build log: `BUILD_R1.log`, SHA256 `c07d544601c169526a1686e98fd8f28da366d3794cae66d24d607889609d521f`
- Executable SHA256: `b747ac4dbc93e7d2a8ad00496fecfb83f709acf9ed1d0b924c16a82d1abfc476`
- Shared library SHA256: `5703b7bf571a3131439c29653b07fd0a1cb5bb26d5b12697f1ee58cef87ac8cb`
- `otool -L` confirms the executable links the selected `libglslang.16.dylib`,
  `libSPIRV.16.dylib`, and `libglslang-default-resource-limits.16.dylib`.
- `git diff --check -- platform/macos/detect.py`: rc0.

The binary starts normally: `--headless --help` and `--headless --version`
both return rc0. The identity probe deliberately returns rc81 because the
runtime identity record is explicitly fail-closed:

```json
{
  "status": "unavailable",
  "reason": "nonbuiltin runtime compiler identity unavailable"
}
```

The record is preserved in `IDENTITY_RECORD.json` (SHA256
`403592f83d52c85532334584d2f53788126d0f26d6bcb297cdac576c0c24e8d8`). This
route therefore proves a real external dependency closure and startup, but it
does not produce a schema-1 compiler identity. No schema-1 stamp, operation,
completion, readback, Ready, or G1 qualification is claimed.

## Boundary and queue disposition

The earlier shared-route receipt remains the provenance for the missing
`ResourceLimits.h` failure. R1 removes that local header/library path defect;
the remaining policy boundary is intentional: `core/engine_build_identity.py`
publishes schema 0 / unavailable for nonbuiltin runtime compiler identity.
That fail-closed behavior must remain until an owner supplies an authenticated
external compiler identity contract. The E06 queue therefore advances the
nonbuiltin build path while retaining the identity item as a precise owner
boundary. Other native work (static target variants, K04/K05, E20 adoption,
and A/U08/Ready/G1 joins) remains active independently.
