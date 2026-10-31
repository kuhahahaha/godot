# Particle recorded-dispatch boundary — 2026-11-01

## Source and build identity

- Source checkout: `/Users/macbookair/Documents/Work/godot_ANGE`
- Commit: `0eac04f63e8ec3dd345748ed2c331e5ef701aa83`
- Build: `scons platform=macos target=editor tests=yes arch=arm64 -j3`
- Binary: `bin/godot.macos.editor.arm64`
- Binary SHA-256: `f115644f8a0aa0b08d2ff8af8521994440b9f6654242304965a0b76c54cd4a6f`

## Verified

- `bin/godot.macos.editor.arm64 --headless --test --test-case='*ParticlesReadback*,*ParticlesProcessRetry*' --quit-after 1`
- 12 test cases passed, 62 assertions passed.
- `python3 tests/servers/rendering/fixtures/particle_process_failure/run.py`
- 9/9 source-model dispatch failure/retry cases passed.
- Commit `0eac04f63e` increments the TESTS_ENABLED dispatch counter only when `_qualification_dispatch()` receives a valid recorded graph handle. This keeps upload-failure/no-dispatch and retry/dispatch observations distinct.

## Explicit remaining boundary

- `GODOT_TEST_PARTICLES_PROCESS_RD=1 ... --rendering-driver vulkan ... --test-case='*production buffer upload retry*'` reaches the opt-in case but fails because the headless test host has no initialized `RenderingDevice/RendererCompositorRD`.
- This does not invalidate the ordinary 12/62 tests. It identifies the next integration task: a real host/main-loop entry with normal backend lifecycle and safe teardown. No global RenderingServer teardown or test-only singleton replacement was added.
- No claim is made for native submit, same-request completion/readback, physical retirement, Metal pixel qualification, K03 typed-close, Ready, or G1.
