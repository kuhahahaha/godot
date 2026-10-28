# macOS `template_debug` static aggregate receipt

Date: 2026-10-29

## Result

The target-specific static producer completed with `tests=no`:

- build: `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=template_debug dev_build=yes tests=no library_type=static_library`
- build rc: `0` (`/tmp/typed-template-debug-static-build-no-tests.rc`)
- no-op/regeneration rc: `0` (`/tmp/typed-template-debug-static-noop.rc`)
- archive: `bin/libgodot.macos.template_debug.dev.arm64.a`
- archive bytes: `1,893,366,248`
- archive SHA256: `b3fb7e2a2d1476e0744e6e3c43fb97ef067450360145ac1a6a91a3c72b2271e7`
- archive members: `2,458`
- nested archive members: none
- `nm` rc: `0`
- required symbols: `get_engine_build_identity` (mangled core symbol), `libgodot_create_godot_instance`, and `libgodot_destroy_godot_instance` all present.

The direct identity host linked against this archive and returned rc0 with exact schema/target identity:
`schema=1`, `compiler_route=BuiltinGlslangV1`, `platform=macos`, `architecture=arm64`, `target_kind=StaticLibrary`, and the generated `core_stamp`/`compiler_stamp` from the current target selection. The host binary is under `static-template-debug-identity-host/host`.

## Boundary

The first `tests=yes` attempt reached the static platform archive but stopped at an existing mixed-tree test/header mismatch (`VirtualProbe::clear_internal_extension` absent); this is recorded in `/tmp/typed-template-debug-static-build.log` and is not a production archive failure. The successful target build intentionally used `tests=no` and therefore makes no test-suite claim. A broader C ABI lifecycle host was not linked: the direct attempt is missing Vulkan loader symbols from the static archive. That is a separate adapter/runtime link boundary, not a fabricated success.

This receipt covers the `template_debug` static producer and identity consumer only. It does not claim project lifecycle, GPU execution, owner association, operation/completion/readback, Ready, or G1 qualification.

Machine-readable checks: `STATIC_TEMPLATE_DEBUG_ARCHIVE_CHECK_R1.json`, `STATIC_TEMPLATE_DEBUG_INPUTS_R1.json`, `STATIC_TEMPLATE_DEBUG_HOST_CHECK_R1.json`.
