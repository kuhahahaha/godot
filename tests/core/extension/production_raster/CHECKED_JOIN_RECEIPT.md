# Prepared raster checked-frame join

Verified on 2026-11-06. This adds CPU/mock-driver integration coverage to production implementation `3ee60f924d`; no production source or ABI change was needed.

Both explicit end and callback-return automatic end execute through the original installed receiver, actual received request/physical context and prepared revision. The fixture uses the actual render-pass graph callback and real RD framebuffer/cache creation, then the original RECORD consume, segment acknowledgment, bounded OBSERVE, graph mapping, native issue/completion, resource retirement and final release. It never clears the graph or marks an observation complete manually.

With this driver's TEXTURE_OUTPUTS_REQUIRE_CLEARS trait disabled, there is no pending-clear preparation command. Each branch must retain exactly five original graph rows: two kind-3 viewport/scissor rows, kind-5 begin, kind-6 attachment clear and kind-7 end. Each fact keeps action 0 and its original canonical/handle identity. Native aliases retain the original per-graph parent relationship and every graph has a covered alias. Native owner references stay held until final release, then retains equal releases.

The draw pass uses depth CLEAR load semantics and does not bind a pipeline or issue geometry. Actual CPU-driver framebuffer and lazy render-pass returns must occur while depth remains live and before the original framebuffer free event. Depth free requires those driver returns plus the original event. Both frame-close branches then settle all four request-owned physical cells through the same bounded service path.

Independent read-only review found no blocker in frame closure, original graph/native mapping, source retention or retirement. This is not GPU evidence or a visible Sprite draw. Pending-depth-clear trait, unknown/failed close, real GPU readback and procedural geometry remain separate acceptance branches.

Commands, all exited 0:

```sh
scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3
bin/godot.macos.editor.arm64 --headless --test --test-case='*RasterJoin*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*ProductionRequest*'
bin/godot.macos.editor.arm64 --headless --test --source-file='*test_production_native_observe.cpp'
bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gpu_particles.cpp'
```

| Check | Cases | Assertions |
| --- | --- | --- |
| RasterJoin (two subcases) | 1/1 | 4,972/4,972 |
| All ProductionRequest | 62/62 | 50,287/50,287 |
| Production native observation | 22/22 | 17,617/17,617 |
| Particle CPU tests | 18/18 | 90/90 |

Final incremental build took 10.96s. The full ProductionRequest group emitted the same three expected negative pipeline-create diagnostics; the other groups emitted no engine errors. The existing MoltenVK deployment-target linker warning remains. Exact source, binary, controlled selection and log hashes are in CHECKED_JOIN_RECEIPT.json. A's separately exported fixed 3ee60f924d editor/receipt remain untouched.
