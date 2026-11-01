# R1 gated fixture review

2026-11-01. Reviewer: `r1_protocol_review`. Read-only fixture/control-flow review; no compiler, builder, test binary or production edit was run by this reviewer. This note does not report runtime GREEN or complete R1 qualification.

Final reread pins:

- `test_wrapper_r1_gates.cpp`: SHA-256 `734d23a22867100182d87182634b21a6f32c5ad9524980cc447d2cdaba87deb7`.
- `advanced_candidate/servers/physics_3d/physics_server_3d_wrap_mt.cpp`: SHA-256 `58ee7a623badabd3b6b0beb2ae6be116eb5c2db143a2062e67502ed1cdafe96f`.
- `advanced_candidate/servers/physics_3d/physics_server_3d_wrap_mt.h`: SHA-256 `37ca87d35cea4d9de65c6847cc63e26f6717f03604a48472b36806b38a958acb`.

The advanced wrapper delta from the frozen candidate is TESTS_ENABLED friend/callback instrumentation at ROOT_ADMITTED, BEFORE/ENTER/AFTER_BARRIER, DRAIN_AFTER_FLUSH and BOUNDARY_ENTRY. Hooks used to block execute outside the metadata mutex. The real wrapper, CommandQueueMT, WorkerThreadPool and native threads/semaphores remain in use; the backend records calls.

## Finding corrected during this review

The first fixture version could observe the correct negative-control violation but did not prove its claimed **pre-danger** boundary. After early park returned, Main could enqueue the impossible barrier before the controller's 86 exit. After injected early ack, Main could enter backend sync and shutdown drain while the pump still held trailing cleanup.

The final fixture adds a never-posted `negative_hold` gate. In early-park mode Main remains inside BEFORE_BARRIER after the real `_acquire_park`; it cannot enqueue the FIFO barrier. In early-ack mode the existing AFTER_BARRIER hook invokes the real `_acquire_park` to publish the legitimate post-barrier request, then remains inside that hook even if the injected ack makes acquisition return. It cannot reach backend sync or the finalizer. The independent controller needs no further Main progress to snapshot the invalid serial/ack and exit 86. No additional production hook is needed.

The negative_hold timeout remains infrastructure exit 87; it is not accepted as the intended 86 result. This fixes the evidence-ordering issue without changing either positive control.

## Four-case logic assessment

1. **Pump-origin stop / four Main boundaries.** The backend signals only after both pump finish requests return and it records that physical finish has not occurred. Main then invokes the selected lifecycle boundary. Assertions require no backend sync/end_sync, exactly one finish, and the same actual worker task and OS thread as init. The repeated finish check happens after the first finish assertion, so it cannot supply the missing first success.

2. **Barrier and trailing cleanup.** BARRIER_ENTER holds the actual barrier before setting doing_sync. The controller enqueues a Ref payload behind it and drops its own references while the barrier is still held. Thus the destructor gate can only be reached by the real queued payload cleanup after that barrier. A foreign finish request is made during the barrier hold. Only after releasing the barrier does the controller wait for cleanup entry and the actual PARK request, then verify ack/park/backend sync are still absent. The cleanup release is independent of Main. The two env controls produce distinct 86 markers from invalid state observations; the default path does not inject either state.

3. **Late publisher / release tail / sealed finish.** ROOT_ADMITTED blocks the foreign producer after incrementing its real ticket but before staging. Main is already holding a normal sync lease and begins shutdown. The first DRAIN_AFTER_FLUSH observation requires one publisher and no setter execution. Its controller lets the producer publish and return before Main resumes the drain, avoiding a controller waiting for Main to consume while Main waits for that controller. Queue ownership retains the payload until Main dispatches it; its destructor performs free/finish under the active drain. The backend's end_sync uses a real accepted RELEASE ticket to queue an internal tail, which only a post-end_sync drain can consume. Counts/order and the existing park serial assert tail execution before pump finish and lease reuse. FINISH cleanup's free/finish cannot add a third backend free.

4. **Rejected admission / three Main boundaries.** BOUNDARY_ENTRY is after the initial service and before Scope construction. Controller finish is foreign and completes before releasing Main. Therefore admission necessarily sees the stop request. The finish assertion precedes the redundant cleanup finish, so the latter cannot conceal an exit-service omission.

For these final positive schedules, no definite fixture-internal deadlock, false-pass path or data race was found by source inspection. This statement is conditional on the reviewed implementation, and is not an execution result.

## Shared-data and lifetime checks

- Cross-thread readiness and concurrently sampled backend counts use atomics. Snapshot accesses wrapper state under its control mutex. Semaphore posts/waits order gate-protected handoffs; thread joins order final ordinary-bool/result reads.
- Backend configuration writes precede command publication or finish control transfer. init identity is read after synchronous init completes; finish identity/order are read after wrapper finish has joined the pump. Ordinary order fields are not sampled concurrently by the controller.
- The hook pointer/context are installed before these cases publish any command that can use them. They are never reconfigured while such a callback is running. This fixture-specific ordering is essential: the plain pointer setter is not a general thread-safe hot-swap API.
- Stack contexts outlive producer/controller joins and wrapper finish. Queue payloads retain their RefCounted objects after producer locals are destroyed. No hook holds the admission mutex while waiting on a gate.

## Limits and next receipt

These fixtures add specific evidence candidates; they do not cover RUN borrowed-worker synchronous getter progress, PARK cooperative-task exclusion, same-thread/different-task release ancestry isolation, returned-holder copy cleanup, the full generated/custom stopped-default surface, or both real backend implementations. The end_sync-tail fixture deliberately stages an internal release through friend access; it proves wrapper drain placement, not a real backend holder's retirement implementation.

Execution should record default positive exit/result plus separate early-park and early-ack runs with exact 86 marker matching. Any 87 gate timeout is a failed/unreached fixture, never RED success. Parent owns syntax/build/runtime receipts and source/binary matching. The frozen base implementation was not modified by this review.
