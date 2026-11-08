# Checked sink terminal reducers, 2026-11-08

Implementation scope: original sink identity/frame/epoch and exactly-once record/cleanup accounting; actual `contains_record` checks; sticky original effect/error preservation; unknown/incomplete/restoration/open-list/unaccounted-prefix quarantine; terminal-only close. Explicit empty/refused Begin can freeze and return known-no-effect custody without native success. Sticky business error no longer prevents the separate cleanup End. Unknown End freezes custody without retry.

The fixed 4a0c owner-Ref constructor and same quoted framebuffer/LIVE-pair revalidation were merged into the local sink while retaining the native callbacks.

## Validation performed

Three targeted objects compiled successfully with `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`: rendering_device, height_capture_checked_sink and test_native_height_range. `git diff --check` passed.

Two added doctest cases directly exercise controlled reducer sink state: failed pre-effect Begin/known-no-effect return; cleanup enter despite sticky business error; unknown cleanup quarantine preserving original error; foreign valid-looking handle; epoch mismatch; duplicate enrollment; deferred bind without invented graph member; unsupported raw transfer quarantine. These are reducer controls, not actual checked backend-call evidence.

## Not executed / not proven

The new tests have **not run** in a relinked host. The current disk has about 934 MiB available; parent requested targeted objects and no full link/reconfigure. After a matching full build, run `bin/godot.macos.editor.arm64 --headless --test --test-case='*NativeHeightRange*' --reporters=xml --out=<receipt>/height.xml`.

No GPU run, actual checked-call active scope, worker/hidden source join, full geometry pass, producer/reader last use, or native completion is claimed. Prepare still reuses the clear-only fixed graph/source capacity; general finite capacity and pre-effect worker/hidden exclusion/actual support remain required subsequent implementation. A raw transfer encountered after append is quarantined; that quarantine is not a substitute for the future original pre-append dependency admission. The full geometry contract remains incomplete.

## Finalization guard follow-up

`_native_height_ranges_prepare_finalization()` now returns `ERR_BUSY` while a checked sink has an opted-in range whose terminal close/unwind has not run; it no longer marks that range closed and removes an empty-looking capture. A reducer test retries finalization before terminal close and verifies RD/Frame custody remains present. A second control seeds a settled original source, then injects an Unknown cleanup result; close marks the prefix uncaptured and observe/return refuse retirement despite the previously positive source.

The rendering-device and native-height test objects recompiled successfully with the `tests=yes module_particles_runtime_tests_enabled=yes` targeted command. No relink or test execution was performed under the current disk limit.
