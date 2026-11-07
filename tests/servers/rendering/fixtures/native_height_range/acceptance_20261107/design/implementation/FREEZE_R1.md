# First native-height clear slice: R1 source freeze

Author ran no build, CPU suite or GPU test. Root owns formatting/build/RED/GPU/commit. Source pins and full four-file copies are in SOURCE_FREEZE_R1.json and source-freeze-r1/. Existing quote/reservation/projection fields were preserved. GPU author's fixture and register_types changes are independently owned and excluded here.

Write set: new servers/rendering/native_height_range.h; existing RenderingDevice h/cpp; new tests/servers/rendering/test_native_height_range.cpp. tests/SCsub recursively glob-includes .cpp and force_link_builder discovers TEST_FORCE_LINK(test_native_height_range); no SCsub edit needed. SCU has its existing separate generated membership mechanism and must be verified by root if required.

## Implemented narrow recipe

Private original RD prepare/record_depth_clear/close/observe/return. Finite max64 active cells; preallocated graph/source/edge arrays, source bound from original driver MAIN_SLOT/1. Graph bound derives checked begin arrays plus end PREFIX_CAPACITY. Frame custody precedes checked begin. Every actual begin/clear/prep/prefix/end handle is saved/deduped; owned checked end runs once only for this exact active begin. No caller-supplied graph or source IDs exist in production API.

Pre-effect refusal: foreign/non-LIVE/mismatched witnesses, non-main/non-RT/unconfigured/failed/closed frame, active draw/compute/raytracing list, callback graph, finalization, native-disposal active continuation; nonstandalone/non-D32/non2D/non-single-depth-layer/mip/sample/shape, owner/view/shared fallback, transfer marker, pending_clear, external access, native retire request, production disposal uses/request, qualification physical attachment/users/dependency, FB invalidation callback, mismatching cache/attachment/depth driver identity, extra color/multipass/VRS attachments. Unrelated Frame hidden reports are never attached; hidden/upload/worker support is not implemented.

Ordinary _end_frame and bounded maintenance graph end call _native_height_seal_frame after actual graph end. It validates EVERY real handle mapping, exact slot AND nonzero command buffer; all one-pass members must map consistently. Ordinary execute_chained_cmds and bounded phase2 issue call the same actual source reducer. Empty BUSY is no attempt. Nonempty attempted prefix is never reissued to fix metadata. Original source facts remain immutable; only original retained serial polling/matching callback can establish complete. Every created range retains original RD custody despite client Ref drop.

Issue attempts retain immediately. Both ordinary _stall_for_frame and bounded _native_disposal_stall_step independently retain sources before any original native recycle/hidden return/fence retirement. BUSY leaves exact pending original owner intact. Failed/unknown/missing/capacity-overflow prefix fails closed; original Frame cannot recycle it. The two epoch-reset paths drop Frame refs only after original successful stall; original RD range refs continue until explicit observer return.

Explicit return requires native_complete or a fully proven zero-native-effect cell (no effect, no unknown prefix, no graph/source/edge). Release BUSY preserves exact cursor; each success releases once. Snapshot retains append/issue/completion/return errors separately and cached complete does not repoll a returned source. No destructor driver work, physical resource free or C refund exists here.

## Finalization and P1 zero-effect correction

Native finalization closes future prepare/record. Original destructive frame/worker failure fail-stop remains unchanged; device-loss notification never becomes Complete/Returned. After successful original frame/worker drain, native_height_finalization_drained records the phase. Range return BUSY preserves driver/cells/native_finalization_returned=false; repeated finalize skips original flush/issue and resumes only range return before graph/resource/queue/driver teardown. Original LAST_REF and RT-exit require_close_success still prevent a void refusal falling into member destruction.

P1 root cause: a sticky pre-effect checked-begin append error made finalizer observe return before legal empty cleanup. Fixed by complete known-noeffect proof before explicit return, while sticky append_error remains in future snapshots and native_complete remains false. Unknown/partial prefix cannot use this path. New minimal CPU counterexample exercises both branches.

No new flush-wide cursor was added: original _end_frame has graph/segment-closed guards, _execute_frame has fence_signaled guard, and _recording_error rejects appends to a closed graph. A CPU counterexample now calls the synchronous getter while retain-BUSY, then repeats end/execute and checks zero additional command/segment ends or native issues.

## Registered CPU controls (not executed by author)

1. Exact slot/buffer mismatch and empty BUSY do not attach or mark attempt.
2. Original completion serial/IssuedUnknown refinement; wrong serial or missing original refinement remains BUSY; explicit release BUSY/cursor/repeated return.
3. Native-error source preserves failed observation and original finalization driver/cell.
4. Retain BUSY and two cells sharing a synthetic original source retain/release independently.
5. Recycled Frame epoch cannot retarget old cell; empty close never native_complete.
6. Known no-effect append error settles custody, preserves error, makes zero driver calls; unknownprefix remains unresolved.
7. Actual ordinary execute_chained_cmds and bounded maintenance issue branch enrollment plus BOTH actual stall retain barriers; source-retain deletion and issue-hook deletion are meaningful negative controls.
8. Sealed synchronous getter retries cannot append/end/issue again while retain is BUSY.

The CPU controls deliberately seed exact synthetic sealed members for reducer/issue/barrier tests; they are not an actual graph mapping proof or actual GPU evidence. Missing execution coverage at freeze: actual specialized record+every-handle seal counterexample in CPU, finalizer post-drain retry counter/child-process LAST_REF, finite-capacity/foreign/preflight branch table, actual hardware normal/bounded last-use. Root's independent new GPU fixture will exercise real D32 clear + handles + both issue modes and LIVE resources before physical retirement. Additional populated height draws, hidden batches, workers, readers, Scene HeightFrame/content and C adapter runtime integration are intentionally later slices.
