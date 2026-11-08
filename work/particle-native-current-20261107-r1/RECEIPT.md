# Native particle runtime Metal continuation

The existing `particle_runtime_native` fixture was rebuilt with the current source and the default-off `particles_runtime_tests` module enabled:

`scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2` → exit 0 (116.27s).

The pinned host is `editor.arm64`, SHA256 `2268e5108732f5295dc174743a1f5e92177725df652ef3b6279ef645632f9efb`, with matching controlled metadata SHA256 `07a1bb8dc2513b3172ffad9602541e245d481ab04f07f196bfd27e5422239a1d`.

`run_native.py` then ran the actual Metal Mobile path with safe render thread, 64×64, fixed 64 FPS and dummy audio. The runner returned `gpu_pass: true`; `RESULT.json` records Apple M3 Pro (Apple9), 390/390 checks, 31 allocation identities, 29 native-resource observations, 6/6 ordinary readbacks, 2/2 SDF readbacks, 13 exact expected engine diagnostics, zero failures, and `released: true`. The outer evidence checks all passed: actual Metal startup, native/outer success, no timeout/leak/fatal, unchanged host/metadata and unchanged source.

This proves the existing native particle retry/readback/physical-return fixture on this source-matched host. It does not qualify the ANGE holder, product request, whole output release, typed Scene phase, or the 117/1290 native run.
