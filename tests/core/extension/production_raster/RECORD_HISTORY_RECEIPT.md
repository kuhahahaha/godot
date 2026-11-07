# Production RECORD history verification

Status: implementation, independent source/test review, full editor build and CPU runtime verification passed on 2026-11-06. This is a CPU contract boundary in the continuing Production raster goal, not GPU acceptance.

`device_session_production_record_history_v1` authenticates the original live request, current RECORD control serial and generation. It reports FIFO admission, executing/returned/consumed state, retained actual results and monotonic control/request history. Consume preserves history; the next accepted control invalidates the previous identity. Query does not schedule, consume or grant recording authority.

Native effects include resource creation, preparation and disposal. Append history separately covers original effect-capable upload/copy/compute/readback, raster begin/end and list closure. `TERMINAL_NO_APPEND` may coexist with native retirement obligations. It cannot revoke Runtime's already-issued `AppendStarted` or reset its once/window; those decisions remain with the original owner. Later empty controls cannot erase earlier append history.

Independent read-only implementation review found no blocking issue after separating append and native histories. It checked exact original identity, FIFO all-or-nothing publication, partial-child failure retention, factory-only allocation, callback/list closure and terminal flags. Independent test review confirmed real bounded CQMT pressure, actual typed compute/partial-upload paths and the registered public query. Both reviews were static and did not claim runtime results.

Focused tests cover empty controls, real FIFO refusal and same-serial retry, returned/consumed persistence, rejected identities/output clearing, actual compute and 64/96-byte partial upload, cumulative history across empty controls, factory success/driver refusal/preflight refusal and cancel without premature native free. The neighboring factory fixture subsequently drives its original OBSERVE/retirement route.

Verification commands (run from the worktree):

```sh
python3 -c 'from core.extension import make_interface_header as m; m.run(["core/extension/gdextension_interface.gen.h"], ["core/extension/gdextension_interface.json"], {})'
scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3
bin/godot.macos.editor.arm64 --headless --test --list-test-cases --test-case='*RecordHistory*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*RecordHistory*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*ProductionRequest*Raster*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*ProductionRequest*'
bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gpu_particles.cpp'
```

The earlier build `/tmp/b4-record-history-build.log` failed with three undeclared append enum identifiers after new source was read against the previous generated header. Header generation was rerun after all source/tests froze; `/tmp/b4-record-history-build-v2.log` tracks the replacement build. No failed build is counted as runtime verification.

Limitations: CPU fixtures use a mock RenderingDeviceDriver. Real raster draw/readback, Unknown/post-entry rejection, frame-return close failure and a FIFO CANCEL control are not covered by the new focused cases. Independent raster resource preparation, successful Production draw, real GPU completion and the Sprite LE192 chain remain outstanding.

Verified results (all commands exited 0):

| Check | Cases | Assertions |
| --- | --- | --- |
| RecordHistory (listed before execution) | 4/4 | 1,675/1,675 |
| Production raster | 2/2 | 146/146 |
| All ProductionRequest | 60/60 | 38,304/38,304 |
| GPU particle CPU tests | 18/18 | 90/90 |

Full build completed in 8m13s. The linker emitted its existing MoltenVK macOS 12.0-versus-11.0 target warning. Full ProductionRequest emitted exactly three expected `!pipeline.driver_id` diagnostics from negative pipeline creation cases. Focused history, raster and particle runs emitted no engine errors. This is mock-driver/headless verification, not real GPU acceptance.

A separate compiled generated-header layout check passed: RecordHistoryV1 size 96, alignment 8, member offsets 0/16/24/32/40/44/48/72. Exact source, generated header, binary and log SHA-256 identities are in RECORD_HISTORY_RECEIPT.json. Temporary logs remain at the absolute paths recorded there.

A subsequent identical SCons invocation exited 0 and reported `.` up to date in 8.34s, confirming no source change remained outside the verified artifact.
