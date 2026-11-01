# R1 candidate source review

2026-11-01. Reviewer: `r1_protocol_review`. Read-only source review; no compile, engine invocation or production edit by this reviewer. Parent reports seven original-wrapper cases actually observed RED and an earlier candidate syntax-only success. Neither fact is a runtime acceptance receipt for the revised candidate below.

Reviewed final candidate files under `candidate/servers/physics_3d/`:

- `physics_server_3d_wrap_mt.cpp`: SHA-256 `7b0b2acc0d945440884c24b10bef8d3940096acd200f6475dad4f437d38a514c`.
- `physics_server_3d_wrap_mt.h`: SHA-256 `fa5b90d3769c421a56d1e40b0a70069c33b278616bb0f1ae75e22737f2f1acd7`.

Authority remains native-physics-cleanup Phase 1 / Revision 2 / Revision 3. This review covers wrapper admission, generated/custom scope lifetime, park/barrier ownership, publisher/release drain and finish transfer. It is not a new backend retirement audit or complete R1 qualification.

## Concrete defects found and addressed during this review

### P1: A parked pump's cooperative yield gave unrelated tasks backend authority

Initial `_should_queue()` treated `Thread::get_caller_id() == server_thread` as pump authority. Initial `_thread_loop()` acknowledged PARK and then called `WorkerThreadPool::yield()`. Actual `core/object/worker_thread_pool.cpp::_wait_collaboratively()` executes ordinary tasks while yielding (task selection at lines 539–546 and `_process_task` at 567–568). Therefore an ordinary task could run on the parked pump's OS thread, take a direct generated branch, flush queued commands and call backend concurrently with Main's acknowledged lease.

Merely changing the check to TaskID would create a second deterministic failure: a synchronous getter in an ordinary task borrowed by the RUN pump would enqueue and wait for the same pump suspended beneath that task.

The revised candidate closes both sides: after PARK acknowledgement it uses non-cooperative bounded `delay_usec(1000)` control polling; RUN cooperative yield retains direct access for a borrowed OS-thread task only while `!parked && !main_dispatch_authority`. The authority test and transition use the control mutex. A new PARK request cannot invalidate a borrowed task's already selected direct branch: the pump cannot acknowledge PARK until that task unwinds to the pump control loop. Main therefore cannot concurrently acquire authority. After full flush return the pump now checks control before entering another cooperative yield.

This solution retains bounded parked control polling and uses existing notifications for RUN waits. It is specifically a scheduler-safe implementation of the park boundary; it does not add support for user callbacks synchronously requiring Main while Main waits for park.

### P1: TLS ancestry crossed cooperative task boundaries

Initial `_has_release_ancestor()` matched only wrapper identity. A callback within a pump OWNER scope could enter a cooperative worker wait, execute an unrelated task, and let that task inherit the suspended task's cleanup-release privilege after REQUESTED. This violated the finite accepted-publisher/release boundary.

The revised Scope records `get_caller_task_id()` and ancestor matching requires the same wrapper **and task**. Legitimate callback/holder cleanup stays in its original task. Borrowed unrelated tasks cannot inherit that task's ROOT/OWNER/RELEASE authority. Seal/CLOSED/pump-exited checks still precede all admission, including FINISH cleanup.

### P2: A stop between lifecycle entry service and rejected admission was missed at exit

Initial sync/flush_queries/end_sync called `_service_stop()` at entry, then constructed Scope. If foreign finish changed OPEN to REQUESTED between those operations, Scope refused and the method returned. A refused Scope destructor does not service stop, so that named Main boundary had no exit service and left the request pending until another boundary.

Revised rejected-Scope branches call `_service_stop()` before returning in sync, flush_queries, end_sync and init. Admitted scopes still drop their own ticket/depth before invoking service. Thus neither route waits for its own admission.

All three findings were communicated to the author before the final reread. The hashes above contain the corresponding fixes. No additional definite bug was found in the reviewed **ordinary initialized lifecycle and specified reentrant-finish/release paths**. This is a limited source conclusion, not proof of all interleavings or a passing runtime result.

## Final source checks

- Normal sync sets its in-progress marker while admitted, calls the existing FIFO `push_and_sync(_thread_sync)`, and only then calls `_acquire_park()`. `_request_finish()` closes admission without changing PARK state. Main depth prevents service while its barrier/root scope remains active. A stop joining an existing lease returns from `_acquire_park()` without creating a new serial/barrier.
- Pump ack is outside the entire OWNER/flush scope. Before acknowledgement, a currently running borrowed task also must return. PARK never performs a normal flush; FINISH_ONLY calls backend finish once on the pump task and returns without another flush. Main performs one join and marks CLOSED.
- Generated guard placement is in each override, outside typed helpers. Direct helpers enclose preflush and backend call; the outer guard survives helper-local return copying/destruction. The three enum/bool exceptions are explicit. Custom methods and creators have root admission. Caller-side cleanup after return is not claimed as covered.
- Scope unlinks TLS before decrementing admission, releases the metadata mutex before service, and invokes service only for Main roots. Raw pump owner epilogues cannot finalize. DRAINING prevents recursive finalizer entry. The drain scope itself is not an outstanding publisher ticket.
- Stopped release admission requires a same-task ancestor and is counted. An unrelated root cannot keep creating publishers after REQUESTED. `_drain_publishers()` includes the final accepted asynchronous tail. Backend end_sync is claimed once and followed by another complete publisher drain. Zero and release sealing are tested/changed under the same mutex before authority transfer.
- Inline and threaded finish both encounter `release_sealed` before any nested public free/backend dispatch. FINISH cleanup can quietly observe already-closing state; it cannot enqueue or perform backend free.
- Repeated/reentrant normal sync is rejected before a second barrier; this does not silently define normal sync as another stop-lease reuse operation. Arbitrary invalid lifecycle nesting, self-delete, callbacks waiting for Main during park, and pre-init concurrent use were not newly qualified here.

## Minimum outstanding behavioral evidence

The original seven RED cases do not cover the following. These need deterministic gates/trace assertions before calling this complete R1:

1. **Real WorkerThreadPool scheduling:** force an ordinary task onto the RUN pump's OS thread and call a synchronous generated getter; require completion without queuing onto its own suspended pump. Separately hold an acknowledged Main lease and establish that the parked control path cannot execute an unrelated task/direct backend call. Trace actual TaskID and OS thread identity, rather than treating them as interchangeable. A task-boundary cleanup fixture must show an unrelated cooperatively executed task cannot borrow an OWNER ancestor after stop.
2. **Lifecycle rejected-admission race:** gate after entry `_service_stop()` and before Scope admission, request stop on a foreign thread, then release. The same Main boundary must finish once before returning. Use each of sync/flush_queries/end_sync as a subcase; another Main finish call must not be what makes the assertion pass.
3. **Revision 3 gates:** hold an idle pump before barrier execution, stop while Main waits, and assert PARK remains unpublished. Separately hold trailing payload cleanup after the barrier command and assert park ack/backend sync remain absent until the whole flush returns.
4. **Late publisher and final release tail:** pause an admitted publisher until after Main's empty flush; require its command/free before finish. Have backend end_sync release a holder that produces more native release work, and assert that work drains before FINISH_ONLY. Backend finish reenters free/finish; the trace must show no later backend operation or queue publication.
5. **Return cleanup and defaults:** retain the existing Main-getter-preflush last-Ref fixture, but do not label it return-copy coverage. Add an independently controlled returned-holder copy/local-cleanup case where applicable and explicit stopped output-preservation checks for both FUNC2S methods. Existing generated/custom starter cases cover only representatives.

Use gate observations before hazardous waits and an independent release controller. Timeouts bound failures; they are not evidence that a required transition happened. Parent owns final build/GREEN execution and source/binary receipt. No complete R1, backend-pair/thread-mode, sanitizer, or scene cleanup acceptance is granted by this note.
