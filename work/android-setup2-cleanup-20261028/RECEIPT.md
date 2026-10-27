# Android setup2 extension-failure cleanup receipt

Date: 2026-10-28

## Scope

The Android JNI caller invokes `Main::setup2(false)` after the first setup phase. When a real SCENE or EDITOR GDExtension initializer returns an error, the caller previously entered `_terminate()` and then ordinary `Main::cleanup()`. That ordinary path requires `_start_success`, so the setup2 failure did not reuse the existing setup-abort cleanup ordering.

This change extracts the existing setup error cleanup into `Main::cleanup_setup_failure(Error, const char *)`. `Main::cleanup()` dispatches to that helper when `setup2_extension_failure` records a returned SCENE or EDITOR extension error. The helper preserves the existing cancel/preflight/G1 gate, staged extension/module teardown, server finalization ordering, core cleanup, and main-thread release. The normal successful cleanup path remains unchanged.

No fallback, owner grant, completion, readback, Ready, Verified, GPU, or security-review claim is added.

## Red

The pre-change source contract check was run against `HEAD` and failed because the helper declaration/definition and the `Main::cleanup()` setup2-failure branch were absent. Raw output is in `red.log`.

## Green

- Source contract: `python3 test_android_setup2_cleanup.py` — rc 0.
- `git diff --check` for the owned source paths — rc 0.
- Incremental macOS editor build: `scons -Q -j2 platform=macos target=editor dev_build=yes tests=yes` — rc 0.
- Fresh native regression: `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='*device_exit*'` — rc 0, 133 test cases, 2619 assertions, all passed.

The regression log contains three expected GDExtension callback-scope error lines from negative controls; the doctest summary is green.

Android cross-compilation and runtime execution were not claimed because this host has no Android NDK/toolchain (`aarch64-linux-android-clang` and `ANDROID_NDK_HOME` are unavailable). The Android source path is covered by the source contract and shared cleanup implementation; an Android device/runtime receipt remains open.

## Files

- `main/main.cpp`
- `main/main.h`
- `platform/android/java_godot_lib_jni.cpp` (verified caller; no source edit)
- `test_android_setup2_cleanup.py`
- `SOURCE_MANIFEST.json`
- `red.log`
- `source-contract.log`
- `diff-check.log`
- `device-exit.log`
- `result.txt`
- `SHA256SUMS.txt`
