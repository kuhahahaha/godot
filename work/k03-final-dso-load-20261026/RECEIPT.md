# Final C35/B16 Debug DSO minimal load

This is the single minimal extension-load check requested for the final C35/B16 Debug DSO. It uses a matching `ange.gdextension` library entry, the exact commit-labelled Godot host and the original RmlUi dependency layout. It does not enter a world, issue a qualification request, or claim Ready/Complete/readback.

- Godot host SHA256: `ee08f82d900ef5556baba1c71db0de69dce4fb25de378dd8027d0f346aad549e` (source commit `9808d5211d`).
- Debug DSO SHA256: `99c22545bf4d4e90018c7c2214f2daa896fae3368bc794995f302b181327fd8c`.
- Matching extension config: `project/ange.gdextension`, absolute library path to that exact DSO.
- Rendering: non-headless macOS Metal/mobile, explicit safe thread, Apple M3 Pro.
- Result: rc0, `K03_DSO_LOAD=0`.

ABI/SDK cross-checks for this host are: selected API JSON SHA256 `5d1f7ddc2981d715eb2958e670007de4ed19d2c55f05805593611ca45a318b81`, D generated header SHA256 `8fd9ba08ced7b39aff9f5f532602a381f46c0da193d07e7e38b2ee884fad33de`, and A SDK archive input `2a6c5fcd79077eb20f52532e9e06010584044237`. The load check uses the matching extension entry and exact DSO hash above. This distinguishes binary/configuration loading from the earlier default DSO initialization negative (`GDEXTENSION_LEVEL_DISPATCH_FAILED`, rc79). The successful load proves only that the exact DSO/config pair can load and return from the extension entry; the main-RD qualification flag remains default-false and no production operation, completion, or owned readback was requested.
