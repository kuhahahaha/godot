# Final integration review: actual phase fixture and public API

Verdict: no blocker found in the final fixture/API increments reviewed here. Root's outer-frame closure, arena-order-independent waiting-control consumption and independent overlap sentinels correct the earlier fixture assumptions without changing production acknowledgement or manufacturing completion/authorization.

This is an independent source review, not an independent build or test run. Root owns the sole execution lane. No active edits, build, test or Git operation were performed by this reviewer.

## Reviewed source identity

| Active source | SHA256 at review |
| --- | --- |
| tests/core/extension/test_production_request.cpp | 5288ca971e5ba5603901c269426828fc72bd15482cdad988e8e192202dd931f6 |
| core/extension/gdextension_production_interface.cpp | 7783becd1a31adcac8d24d851cbb7194cc24ed0aca177811bfb37ab1f97738fb |
| core/extension/gdextension_production_window.cpp | 9c5b7cc07f671d8e0f12f14ad465c0e084ecfb2e8410f4f2da3c9a423456cb3e |
| core/extension/gdextension_interface.cpp | 0d8cb446626e144455a99720cf9e8a306bd88e7c8174f587f234993c0d1a550e |
| core/extension/gdextension_library_exit_guard.cpp | ef5e70162c90bf14bc7cdecb28a4ec0720b1c74f6f7551c9b403506e9791c74f |

These identify the inspected fixture and API/lifetime sources, not a comprehensive independent review of every path in the 13-path integration. Root is temporarily removing the actual RS hook for its counterfactual build; this review does not identify that transient hook-removed file as the final production snapshot or claim it is restored.

## Actual frame return and holder acknowledgement

The plain same-owner normal relay is created and retired through the original Callable/holder APIs. The fixture drops its last external Callable alias, checks its actual free once, then calls `fixture.frame.leave()` before observing holder_closed or forgetting the relay. This is the original outer SceneReady frame return, not a synthetic holder acknowledgement. It correctly allows `retire_binding_holder`'s pending retirement on that exact outermost frame to finish.

All required Callable creation and typed leaf installation precede that leave. Subsequent capability/registration/public rejection checks use original owner/enrollment/session gates and do not require a pre-admitted SceneReady frame. RECORD scheduling validates original request/attachment identity and live enrollment; the actual delivery creates its own CallbackScope. No outer pre-admitted frame survives into scheduling/draw, which also strengthens the expiry check.

Plain relay export now requires ERR_UNAUTHORIZED with the complete output sentinel unchanged. The original cancel/release-holder/free/observe/forget sequence removes that plain fixture before the typed registration runs. No internal wire token is assembled to reach an otherwise unexportable plain registration; direct plain-register runtime coverage is not claimed. Production register independently checks the actual typed Callable metadata and exact original token/holder identity.

## Two waiting controls: actual arena order

The no-draw close branch retains the two original request/control identities and a test-local consumed[2] observation array. After each actual unregister/sync turn it attempts public consume for every previously unconsumed identity. An OK return marks only that observed identity consumed, checks ERR_UNAVAILABLE and its matching original request serial/control serial, and contributes to exactly one returned_this_turn. Other outstanding identities must still return Busy. Two turns with exactly one previously unconsumed OK per turn establish both controls have actually returned, independently of arena slot or reservation order.

The consumed array is test bookkeeping, not a production control-state assignment. No mailbox, serial, waiting bit, occurrence or holder fact is injected. While either original mailbox is being closed, actual relay observation still requires !cancelled, !cell_released and !holder_closed; public close output also reports no release. Only after both original controls return does another unregister release the relay cell, remain Busy on the external Callable, and finally observe real holder acknowledgement after its actual free. Both original requests then cancel/release and original owner cleanup runs. This branch does not call draw or the private dispatcher.

The prior reservation-order assumption was a fixture defect. Root's earlier additional close-waiting failures should not be characterized as a production wake-order bug.

## Independent overlap negatives and public guards

The aligned shared key/output buffer is initialized to 0xa5, copied to a full-byte sentinel and compared after the actual-request query rejection. Before each subsequent invalid-request or misaligned-key query, the buffer is restored from that same sentinel. Thus an earlier mutation cannot contaminate a later negative's precondition or make its comparison vacuous. All checks require ERR_INVALID_PARAMETER and full-byte preservation.

The inspected public wrappers validate nonnull/aligned complete object ranges, integer-range overflow and relevant input/output disjointness before dereference, output initialization or Registry calls. Query rejects aliasing before passing key/out to the owner, preserving the input even for an invalid request. The relay wrapper separately rejects null receiver and overlapping Callable/output; its actual positive path installs the original adopted Callable. Capability, relay, registration and unregister successes, duplicate identity behavior and rejection cases use installed original public procedures in this fixture.

The absence of a misaligned-Callable runtime negative is intentional in the pre-fix RED sequence, because old code could dereference that pointer. Source guards were reviewed; this is not claimed runtime coverage of every extent/overflow/misalignment guard.

## Actual hook and cleanup limits

The normal branch still schedules both original RECORD controls through ordinary FIFO first, requires no callbacks and genuine QUEUED states, then uses actual public draw/sync. It verifies max_operations1, original EXECUTING identity and operations, real active normal-relay delivery, actual self-close/release Busy and physical-context expiry after each callback. No TESTS/private dispatcher or frame/phase state fabrication is used.

The count-mismatch recovery closes admission and uses bounded original unregister/FIFO turns to return the two known zero-action requests before original consume/cancel/release, actual Callable free/holder acknowledgement and fixture cleanup. This is designed for root's actual-hook-removal counterfactual. It does not claim generic exception safety when fatal assertions escape from inside a typed callback or when the original FIFO/holder mechanism itself is broken.

Public operation binding still ends in ERR_UNAVAILABLE until the real original exporter is joined. Neither the fixture nor this review establishes successful scene-key binding, source/sample/borrow/retained-content semantics, operation-once facts, RT retained-content budget behavior, actual sphere/height raster, GPU qualification or scene readiness. Those boundaries remain separate.

## Execution attribution

Root reports final GREEN focused coverage of 4 cases/420 assertions and combined coverage of 222 cases/94138 assertions, exit0, no new diagnostics, with the combined suite retaining its original seven deliberate diagnostics. These are root-provided results, not executions by this reviewer.

Root previously reported a normal failed r4 baseline with genuine delivery/expiry but deferred holder acknowledgement, then real RED failures for plain export/query overlap. The frame lifetime, waiting-order and sentinel corrections above explain the fixture changes; they do not justify relabeling failed historical receipts as passes. Root's hook-removal counterfactual and restored final rebuild were still pending at this source review. No result is inferred for them.
