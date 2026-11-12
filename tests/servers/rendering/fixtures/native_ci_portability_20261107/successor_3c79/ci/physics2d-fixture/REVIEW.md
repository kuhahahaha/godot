# Physics 2D pump-origin fixture race

Only `tests/servers/physics_2d/test_wrapper_r1_gates_2d.cpp` changed. No production source, build, runtime invocation, staging or commit was performed. The original CI RED is exact run 42377076633, head 3c79c5d8e107da32c07f1ffa017302de50896380, macOS Editor job 128416888686. Original ANSI log and three first-failure excerpts are in the parent evidence directory. The one failed case has three failures at original line 191: expected live finish_count 0, observed 1 (sync, flush_queries, end_sync).

## Contract and causal counterexample

The existing review `work/physics-wrapper-finish-recovery-20261101/R1_CANDIDATE_REVIEW.md` states that only Main root epilogues service stop; raw pump owner epilogues cannot finalize. Actual production source confirms:

- `Scope::~Scope()` decrements the Main root depth and calls `_service_stop()` for Main roots.
- `step()` holds such a root while queuing the asynchronous backend step.
- Pump callback `finish()` requests stop, but `_service_stop()` immediately returns outside Main.
- Main `_service_stop()` waits for `main_depth == 0`, acquires PARK, drains accepted releases, transfers FINISH_ONLY, joins the pump, and closes.
- Pump FINISH_ONLY performs backend finish; natural pump callback return alone does not grant shutdown authority.

A permitted schedule is: Main queues step and remains in `step()` root scope; pump calls wrapper finish twice, observes count zero, posts `step_returned`, and returns from backend step; Main root epilogue sees the request and services/join-completes the stop before `wrapper->step()` returns to the fixture. The fixture subsequently sees live count one, although both pump-origin finish calls returned without recursively finishing the backend. The callback snapshot remains zero. Therefore the late live-zero assertion is invalid under the existing contract, and the CI evidence matches that schedule. This is a source-derived causal explanation, not a recorded scheduler trace.

## Narrow change

Capture the actual backend finish count in the existing pump callback after both wrapper finish calls and before the existing semaphore post. The existing boolean assertion and the zero-count assertion now consume that published snapshot. The semaphore establishes the phase boundary without adding sleeps or blocking callback completion. The existing final count-one, no sync/end callbacks, init/finish thread and task identity, duplicate-finish and cleanup assertions remain unchanged.

An extra callback release gate held until Main returns from `step()` would deadlock this valid schedule: Main root epilogue waits for pump PARK while the pump waits for Main to release the gate. No such gate was added.

## Regression sensitivity and verification limits

If a production change finishes recursively inside either pump-origin `finish()` call, the captured count is nonzero and both snapshot assertions fail. If shutdown is omitted or duplicated, the existing post-boundary exact count and identity checks fail. The helper uses the real production wrapper and actual backend finish counter.

`git diff --check -- tests/servers/physics_2d/test_wrapper_r1_gates_2d.cpp` passed. Pre/post source hashes are recorded in SOURCE_IDENTITY.json; PREIMAGE.cpp.txt preserves exact input. No GREEN runtime or mutation execution is claimed: parent explicitly prohibited build/runtime in this author task. Parent verification remains necessary.
