# SDK62 original lookup dynamic fixture

This standalone macOS fixture compiles a real GDExtension against the fixed
three-product custom interface SDK (`--interface-sdk-root`) together with the
supplied godot-cpp SDK's `device_exit_api.hpp` (`--sdk-root`). It calls
`godot::internal::load_gdextension_interface` on the generated ordinary loader
and `DeviceExitApi::load(original_get, true, true)` plus guarded per-entry
Storage generation-finality capability lookups directly from the original entry
callback. No lookup mock, replacement header, SDK library, engine build, or public
ABI modification is involved.

Requirements: Python 3.9+, `clang++`, a native macOS Godot executable, the fixed
three-product custom interface SDK (`misc/ange_native_sdk`), and the godot-cpp
checkout containing `device_exit_api.hpp`. The verified lifecycle SDK revision is
`62b1b20c2ddeddaca0236e9651c66a2f5727e83e`; this test requires its frozen
41-function lifecycle load surface plus the two guarded Storage generation-finality
capabilities (`device_session_qualification_storage_domain_install_v1` and
`device_session_qualification_storage_generation_finalize_v1`, 43 raw lookups in
total). The current implementation supports macOS only. Linux and Windows
compilation/execution are not claimed.

```sh
python3 -B tests/core/object/fixtures/device_exit_sdk_load/run.py \
  --sdk-root /absolute/path/to/godot-cpp \
  --interface-sdk-root /absolute/path/to/misc/ange_native_sdk \
  --binary /absolute/path/to/godot \
  --output /tmp/sdk62-load-run
```

`--output` must name a new directory; omitting it creates a temporary directory.
The runner freezes the supplied binary to an identical executable snapshot before
running, so a concurrent rebuild cannot silently replace the tested host. The
generated interface header and ordinary loader come first from
`--interface-sdk-root`, while `device_exit_api.hpp` is included from
`--sdk-root`, with input hashes checked before and after compilation/execution.
`RESULT.json` records commands' artifact hashes, input hashes, host version,
platform, and pass/fail status. Each command has JSON and stdout/stderr logs.
Nonzero status, stderr, missing observations, unexpected API results, and
incomplete lifecycle/shutdown fail the runner. Each subprocess has a 60-second
timeout.

| Case | Required observations |
| --- | --- |
| Guarded (`device_exit_guard_version=1`) | All 43 raw addresses non-null (41 frozen SDK62 + 2 Storage generation-finality capabilities); ordinary generated loader succeeds with zero `device_session_` lookups and leaves both new globals null; capability filter rejects missing either procedure; SDK load true; unrequested SceneReady V2 member null; original guard registration and SCENE initialization/termination (plus EDITOR when present); preflight publish; real Manager load and explicit unload both successful. |
| Legacy (`device_exit_guard_version=0`) | All 43 raw addresses null; ordinary generated loader succeeds with zero `device_session_` lookups and null globals; finality capability check false; SDK load false; ordinary initialization and normal process shutdown complete; no guarded API calls. |

The probe reuses the original guard/preflight/lifecycle protocol documented in
the parent fixtures directory. Guarded release must complete before its script
exits. Legacy uses normal shutdown because an unenrolled legacy extension does
not acquire the guarded explicit-close contract.

Only eight distinct lifecycle functions are invoked. Looking up all 43 functions
and successfully loading their typed members does not establish SceneReady,
session/qualification business operations, native rendering, GPU completion, or
cross-toolchain DTO layout compatibility. Removing a required native export or
changing guarded lookup selection makes the guarded case fail; exposing a typed
export to legacy makes the legacy case fail. Omitting or failing any recorded
lifecycle step also fails verification.

`probe.cc` deliberately uses `.cc`: `tests/SCsub` recursively collects `.cpp`
files, and this source must stay outside the engine test link. Run the script
explicitly; merely building/running doctest does not execute this fixture.
