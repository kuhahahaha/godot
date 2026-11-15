# Particle collision texture changed RED/GREEN receipt

Root ran the frozen fixture on the original main RenderingDevice: Metal 4.0, Forward Mobile, Apple M3 Pro (Apple9). RED returned 1 with 98 checks and 16 failures. GREEN returned 0 with 98/98 checks passing. Each run contains exactly one result record and 20 observations, all reporting actual main-RD render-thread ownership and valid collision owners. The copied run logs contain no engine/script error diagnostics. Return codes were reported by root; the raw logs do not contain exit-code trailers.

The source baseline is `60d14b9c63195c96137d714ff41e681ffbb1a247`. RED used the two old node implementations with the frozen fixture and read-only observer; camera tests had not yet been included. The RED executable and full tree snapshot were not retained, so its executable hash is recorded as unavailable. GREEN includes the adjacent camera implementation/tests. Per-file baseline/current source hashes, per-file diff hashes, the particle-scope diff hash, the combined tracked diff hash, and the unchanged fixture hash are in [RECEIPT.json](RECEIPT.json).

Both nodes now disconnect the old texture on reference replacement, subscribe to `changed` using `CONNECT_DEFERRED`, and refresh using the current member. Repeated same-reference setters still refresh. Root added `core/object/callable_mp.h` after the first GREEN compile identified an undeclared `callable_mp`; the successful GREEN build log follows that correction.

Build command (both stages):

```sh
scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3
```

Runtime command (both stages):

```sh
bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/particle_collision_texture_changed --script run.gd
```

RED failures comprise four clear-stage current-RID mismatches, four rebind-stage current-RID mismatches, four rebind-stage nonzero-resolved-RID failures, and four missing-subscription failures across SDF collision and vector-field attraction. GREEN passes those same frozen checks, plus shared-resource propagation, repeated same-reference setters, reference replacement/null detach, queued old notifications, receiver destruction, and caller-owned source survival.

This verifies actual collision-owner resource association and nonzero resolved 3D RIDs. It does not verify shader sampling, particle motion, or physical effects. The observer does not call `rd.texture_is_valid()` on the resolved RID. No dummy/local RD or extracted-source substitute is used.

The current GREEN controlled artifact receipt reports 156531640 bytes and SHA-256 `2dde15a04a877b1c93e5cbe92147b0a985dce61a1805232efef75c1df4c313e6`; its copied sidecar is [green.controlled_build.json](green.controlled_build.json). This is a source-tree runtime build, not a new preserved fixed-artifact export.

| Raw log | SHA-256 |
| --- | --- |
| [red_run.log](red_run.log) | `bc9082edf4e6d59eefd95f891bb2c6ba62193cfd36b896ef94ba4c825fc7b737` |
| [green_run.log](green_run.log) | `765361f33bcf55490d5774c0e21cd2f32e58c00171ecfc6e8ab7540bb028b61e` |
| [red_build.log](red_build.log) | `78103fde13f770c3d7d928e734728a6c5185722d31729bbf06ce3ba8d97f93cb` |
| [green_build.log](green_build.log) | `17ddb0550bdab206db17a7ff6d3f7508a506c362f0cb81be4765608f7906eaa7` |
