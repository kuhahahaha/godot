# Checked height worker preflight refusal — 2026-11-08

This slice adds one narrow guard in `RenderingDevice::_check_transfer_worker_operation`.
When an opted-in `HeightCaptureCheckedSink` is active for the current frame and
qualification epoch, the operation returns `ERR_UNAVAILABLE` before worker locks,
qualification source quoting, or native issue/admission. A clear-only native
height range has no capture sink and is unaffected.

The guard is deliberately refusal-only. It does not create a worker watcher,
join a submit interval, retain a completion source, attach hidden reports, or
claim geometry capture/completion. It prevents a raw transfer dependency from
being accepted as a settled height prefix until the authenticated worker join
contract is implemented.

Validation:

- `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2 bin/obj/servers/rendering/rendering_device.macos.editor.arm64.o bin/obj/tests/servers/rendering/test_native_height_range.macos.editor.arm64.o`
- exit 0
- `git diff --check` passed

A linked host test and GPU run were not performed. A focused fixture still needs
to exercise an actual checked VA/IA path with an active sink and a worker-owned
resource; synthetic sink reducers do not prove that call path.
