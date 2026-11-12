# Typed lifecycle dynamic-image fixture

`device_exit_dynamic_library.c` is the existing native entry fixture for three optional tests:

- `device_exit::library-close/manager-real-dynamic-close-fact`
- `[DeviceExit][TypedABI] version one loads an actual dynamic image`
- `[DeviceExit][TypedABI] manager loads guarded SCENE from its declared minimum`

It includes the engine's own generated `core/extension/gdextension_interface.gen.h`.
Build that header from the same checkout as the test host; do not copy a second ABI
header into this directory. An ordinary matching Godot build generates it. To
regenerate only the header from this checkout's interface JSON, run from the
repository root:

```sh
python3 -c 'import sys; sys.path.insert(0, "core/extension"); import make_interface_header; make_interface_header.run(["core/extension/gdextension_interface.gen.h"], ["core/extension/gdextension_interface.json"], {})'
```

From the repository root, build the optional image on macOS with Clang:

```sh
clang -std=c11 -dynamiclib -I . tests/core/object/fixtures/device_exit_dynamic_library.c -o tests/core/object/fixtures/libtyped_dynamic.dylib
nm -gU tests/core/object/fixtures/libtyped_dynamic.dylib
```

The exported symbol must include `typed_dynamic_entry`. The generated image is a
local test artifact: do not stage or commit it. The source remains ordinary C;
`tests/SCsub` discovers engine test translation units with `*.cpp`, so it does not
compile this dynamic entry into the host.

Linux can build the same C fixture with Clang or GCC:

```sh
cc -std=c11 -shared -fPIC -I . tests/core/object/fixtures/device_exit_dynamic_library.c -o tests/core/object/fixtures/libtyped_dynamic.dylib
nm -D --defined-only tests/core/object/fixtures/libtyped_dynamic.dylib
```

The fixed `.dylib` filename is also the test lookup path on Linux; it still holds
an ELF shared object there. Linux execution is not established by a macOS compile.
Windows compilation/execution is currently unsupported by these instructions:
the source uses `__builtin_trap` and has no Windows symbol export decoration.
There is no compiler invocation during the Godot test run.

Run the matching test host from the repository root with each exact test name
above. If the image is absent, each case emits a doctest warning explicitly
saying that its dynamic-image body was not executed, then returns. Such a warning
is not dynamic load/unload coverage. A qualification run must first build the
image and then verify the three cases execute without the missing-image warning.

The historical work source SHA-256 is
`24deba703c5cd47bf0c9908a2fc55920661d3426eaa88b81dcbb1cb01c75d1bc`.
The existing formal source SHA-256 is
`f5d94150c951230cc12f124745b7f798a7b6e22ef5471b7b40ad5b395aa7dbfe`;
its sole difference from that historical source is the include path to the
engine-generated ABI header. This wiring change leaves that source untouched.
