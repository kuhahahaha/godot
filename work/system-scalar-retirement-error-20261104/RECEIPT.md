# System internal-set retirement error propagation

Base: 93f18e5a30 (includes Unknown/capacity fixes). Working-tree repair only; no commit, shared-tree/B4 edit, ABI change, report state/revision/native-code change, engine build or native runtime performed by this worker.

## Behavior

The original System ticket remembers the first non-OK/non-Busy retirement error only when it is System, compute ownership has physically retired, its internal set exists, and that set has no actual free event. The existing Registry mutex protects all production error-field writes/reads; the helper additionally reads free_event under its physical cell mutex. The maintenance callsite runs on the original RT and records its error immediately on return, before the second set-return step can enqueue another driver free.

System observe copies the original compute report and returns the retained Error through the existing call result. Already recorded failure returns before rescheduling; the post-service observation also returns it. System release authenticates the ticket, then returns that error before native-source release, reverse-edge removal, metadata reclamation or ACK. Set-return steps do not enqueue/free another object after the error. A real set-owner mismatch enters the same first-error path.

The compute report (including state8, revision, original native_error/native_code and readback payload) remains unchanged. Call native_code is zero because there is no authenticated new raw code from the Error-only maintenance return; the patch does not infer or copy a stale global code. facts/copy and the original frozen report remain their original source observations.

A set already physically returned at error-recording time is not polluted by a subsequent unrelated maintenance error. Conversely, if a free was queued but had not returned when the error occurred, that error remains sticky even after a later real free event. No timeout, retry, later OK/Busy, alternate error or physical return resets it. This is retained/quarantined ownership, not automatic recovery or a new ACK authorization.

## Changes

- core/extension/gdextension_qualification_internal.h: owner-local remember_system_set_return_error helper.
- core/extension/gdextension_qualification.cpp: immediate post-maintenance preservation before the second retirement step.
- core/extension/gdextension_system_scalar.cpp: observe Error return and stop repeated set-free admission after a retained failure.
- core/extension/gdextension_qualification_ticket.cpp: System release error gate before any release effects.
- tests/servers/rendering/test_system_scalar.cpp: first-error persistence, completed report immutability and non-System/absent/active exclusions.

## Validation and limits

check_retirement_behavior.py executes the real source bodies for the maintenance-result window, set-return step, pre/post-service observe lookup and release admission. Scalar stand-ins supply containers, locks, RD error/free state and a counted post-admission release frontier. The helper body is extracted from production after the repair; the baseline uses its actual service/set/observe/release statements. This is behavior testing, not source substring assertion, but it is not an engine link, native driver run or concurrency test.

- Frozen 93f18e5a30 red: exit1, nine failed behavioral assertions.
- Working-tree green: exit0, failures0.
- Cases: maintenance failure after compute completion, no subsequent set-free enqueue, first-error persistence across alternate Error/OK/Busy, both observe return windows, original report/native code preservation, release before any release/ACK frontier, queued-free failure sticky after eventual physical return, free event already present and free event arriving during maintenance excluded from new errors, pending Busy unchanged, actual set-owner mismatch preserved.
- Previous Unknown/capacity source behavior fixture: exit0, failures0.
- clang++ -fsyntax-only succeeded for system_scalar.cpp, qualification.cpp, qualification_ticket.cpp and test_system_scalar.cpp. Commands and paths are in SYNTAX_RESULTS.json; unused-parameter warnings remain in unchanged headers/test stubs. No engine build/link/doctest/GPU result is claimed.
- git diff --check passed.

BEHAVIOR_RESULTS.json and red/green logs/generated source bodies retain executable evidence. CHANGED_FILES.json binds the reviewed bytes. The single-thread fixture does not establish cross-thread visibility or full teardown liveness; normal linked-engine/native acceptance remains required.

Root independently ran the final green retirement behavior fixture: failures=0. Independent Sol review found no confirmed defect and checked locking, maintenance-before-second-return ordering, observe double checks, and release-before-native-release ordering. No engine/GPU or threaded-lifecycle claim follows.
