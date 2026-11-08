# Checked height worker preflight refusal — 2026-11-08

This slice adds one narrow boundary in `RenderingDevice::_check_transfer_worker_operation`.
After the original worker thread/operations locks are acquired, an operation at
or below `operations_processed` is accepted as an original already-processed
proof and creates no historical native source. An active opted-in
`HeightCaptureCheckedSink` still returns `ERR_UNAVAILABLE` for a pending/current
operation until the authenticated interval join is implemented. A clear-only
native height range has no capture sink and keeps the legacy worker check.

This remains a narrow preflight boundary. It does not create a worker watcher,
join a submit interval, retain a completion source, attach hidden reports, or
claim geometry capture/completion. It prevents a raw transfer dependency from
being accepted as a settled height prefix until the authenticated worker join
contract is implemented.

Validation:

- `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2 bin/obj/servers/rendering/rendering_device.macos.editor.arm64.o bin/obj/tests/servers/rendering/test_native_height_range.macos.editor.arm64.o`
- exit 0
- `git diff --check` passed

A linked host test and GPU run were not performed. The focused reducer now
exercises settled-operation acceptance, pending-operation refusal, lock release,
and clear-only legacy behavior. It still does not prove an actual checked VA/IA
resource path, submit interval join, worker retain/completion, hidden sources,
or producer/reader leases.
