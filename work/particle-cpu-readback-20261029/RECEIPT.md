# Particle CPU readback implementation receipt — 2026-10-29

## Delivered behavior

- Added `RendererParticlesStorage::particles_get_current_data(RID)` and the
  matching `RenderingServer::particles_get_current_data(RID)` binding.
- RD reads back the latest `particle_buffer` after validating the real owner
  RID, amount, trail expansion, userdata stride, overflow, and returned byte
  count. Invalid or unallocated state returns an empty snapshot.
- GLES3 reads the current instance buffer using the same real RID and fails
  closed for empty particle sets. Dummy storage returns empty.
- Added a layout regression covering userdata and trail expansion and a dummy
  fail-closed case.

## Verification

1. RED: before the implementation, the targeted object compile failed because
   `particles_get_current_data` was absent from the storage interface:
   `error: no member named 'particles_get_current_data'`.
2. GREEN Debug object build:
   `scons platform=macos target=editor dev_build=yes tests=yes accesskit=no angle=no -j4 bin/obj/tests/servers/rendering/test_particles_readback.macos.editor.dev.arm64.o`
   and affected RD/RenderingServer/GLES3 objects all passed.
3. GREEN Debug test:
   `bin/godot.macos.editor.dev.arm64 --test --test-case='[ParticlesReadback]*' --no-colors --no-intro --no-version`
   Result: **2 passed, 0 failed; 5 assertions passed**.
4. Release compile:
   `scons platform=macos target=template_release dev_build=no tests=no accesskit=no angle=no -j4 bin/obj/servers/rendering/renderer_rd/storage_rd/particles_storage.macos.template_release.arm64.o bin/obj/drivers/gles3/storage/particles_storage.macos.template_release.arm64.o`
   Result: passed.
5. UBSan object targets were already current and accepted by SCons:
   `scons platform=macos target=editor dev_build=yes tests=yes sanitizer=undefined accesskit=no angle=no -j4 bin/obj/servers/rendering/renderer_rd/storage_rd/particles_storage.macos.editor.dev.arm64.san.o bin/obj/tests/servers/rendering/test_particles_readback.macos.editor.dev.arm64.san.o`.
6. Full Debug suite was also run. It reported 1765 passed, 3 failed, 3
   skipped. The failures are pre-existing/unrelated (`test_tcp_server`,
   `test_texture_snapshot`, and `test_metal_completion`); the particle
   readback cases remained green.

## Limits and external dependency

The current ANGE DSO source census still has no production caller for
`ParticleProgramLoader::request_native`, `poll_native`, `acquire_native`, or
`StageExecutor::start_native_qualification`, and no authenticated same-request
particle-to-E09 association. This patch therefore exposes a real CPU snapshot
through Godot's RenderingServer without claiming ANGE qualification, dispatch
admission, completion, GPU authority, or release. An external ANGE producer
must consume this entry before those claims can be made.
