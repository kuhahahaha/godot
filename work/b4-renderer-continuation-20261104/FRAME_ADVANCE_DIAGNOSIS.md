# Original FullBirth frame-advance diagnosis

The supplied FullBirth run used the immutable host under
`work/native-c5e4bc-fixed-host-20261107/`, SHA256
`c5e4bc1951643a6968c69a73d15b19abd0479de174bfa8e5898a4a689a83f939`.
Its log first reports `swap_buffers:12098` after `poll.preparing`. The embedded
Engine revision is `3c79c5d8e`, whose Git object puts that diagnostic at line
11532. The retained host metadata and eight source files do not establish the
exact dirty RD source; no exact reconstruction is claimed.

The original `_stall_for_frame` can return before `fence_wait`: native source
completion mismatch, ticket/production retention, native recycle, hidden source
completion and observer release can all refuse. `_begin_frame` propagates those
results. The ordinary swap path sets `qualification_frame_advance_pending` before
calling it, and `_recording_error` then returns BUSY even when
`frame_wait_error == OK`. Neither logged diagnostic independently establishes an
actual GPU fence failure. The original FullBirth log lacks the return code and
native/fence state needed to identify its precise branch.

Both `d1e5891870` and `ad6b52286b` are ancestors of the embedded revision. The
latter defers ordinary drawing for a production-owned native disposal cursor;
it does not resume an ordinary pending frame before renderer state advances.

The exact fixed host was run only for the CPU case
`[R01][FrameWait] ordinary advance retries an early native wait before rotating again`:
exit 0, one case and 29 assertions passed. This proves its existing same-slot RD
retry mechanism, not FullBirth acceptance.

New tests drove the real ordinary `RenderingServerDefault::draw` boundary after
the original swap created a pending slot. Before the server change, both new
cases failed: 148 assertions, 71 failures. Callbacks ran, compositor state advanced
and redraw demand was lost before the slot recovered. The minimal server change
retries the same original `_begin_frame` first, preserves callbacks/redraw on
healthy BUSY, and reports actual error/issue/wait/native state for real failures.
The first combined FrameWait/OwnedFrameDraw GREEN was 22 cases and 1,959 assertions.

A second tests-only control observed the engine error handler directly: the
original swap issued two erroneous ERROR diagnostics for healthy early BUSY.
Actual fence BUSY must remain an error because the original fence path latches
`frame_wait_error`. Final verification and the fixed successor identity are
recorded with the integrated package; no competing ANGE/GPU run was launched.

Final CPU GREEN: 23 FrameWait/OwnedFrameDraw cases and 1,989 assertions, exit 0. The new actual-fence fixture initially stopped at the debug error trap before assertions. A follow-up debugger run with SIGTRAP continuation passed its original 20 assertions: the public swap_buffers error diagnostic, rather than damaged fence state, triggered the trap. It now explicitly enters the retained pending fence state and verifies the real `_begin_frame(true)` return ERR_BUSY, retained wait error and successful recovery. No production error condition was weakened. The original public healthy-BUSY error-handler assertions remain covered separately.
