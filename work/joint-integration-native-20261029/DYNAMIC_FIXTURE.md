# Dynamic-library test fixture prerequisite

The three real dynamic-library cases in test_device_exit_typed_abi.cpp and test_device_exit_library_close.cpp resolve `work/typed-lifecycle-abi-v1/native-dynamic/libtyped_dynamic.dylib` under process cwd. Their historical absent-file early return is not passing dynamic coverage. This candidate now includes canonical source `tests/core/object/fixtures/device_exit_dynamic_library.c`; only its include changes from the original work fixture, selecting the actual generated core ABI header instead of a movable work header. It is not linked into engine/release production.

For a fresh macOS source checkout, build the editor first (generating core/extension/gdextension_interface.gen.h), then generate the fixture before claiming these cases or the full group:

```sh
mkdir -p work/typed-lifecycle-abi-v1/native-dynamic
/usr/bin/clang -std=c11 -dynamiclib -I. tests/core/object/fixtures/device_exit_dynamic_library.c -o work/typed-lifecycle-abi-v1/native-dynamic/libtyped_dynamic.dylib
```

The command is first-target macOS only. No foreign-platform fixture claim is made. In an existing tree with frozen native evidence, compile to a private scratch cwd's same relative work/... path and run the absolute editor from that cwd; do not overwrite the prior fixture merely to verify this export.

Actual current validation uses the fixed editor from INPUTS and a newly compiled C fixture under regenerated-fixture-runtime/: DYNAMIC_FIXTURE_BUILD_COMMAND/TEST_COMMAND preserve exact argv/cwd; both rc0, three cases/32 assertions pass. Same library SHA before/after; original repository work fixture was untouched. These are real dynamic load/unload/core close-return bodies, not GPU, E09 Scope or Ready. Their source inputs and generation step are part of the export; the generated local library is pinned but excluded from Git.
