# Independent review: cleanup-only Scene RELEASE budget test

Verdict: no blocker found for the stated cleanup-only unit or its controlled callback-lock counterfactual. The new test exercises an actual retained Ref through original queued RT RELEASE service, distinguishes readiness from reclaim, charges each return attempt, preserves Busy until the content has really returned, and observes final original owner reclaim. It does not establish a Scene binding/sample grant, native-read completion, or maintenance-already-spent credit coverage.

Source review only. Read the test-only diff and surrounding original RELEASE/consume/lifetime code; no active edits, build, test or Git mutation were performed. The temporary production change retaining Registry mutex around return_after_reads is root's controlled counterfactual and is explicitly excluded from the final-change review.

## Source identity and test scope

Reviewed active `tests/core/extension/test_production_request.cpp` SHA256: `a6838690802c0c351e25d5d2c3fcf527d20bceb1a3f408986ab8ffa476c409e1`. Its previous base SHA256 is `5288ca971e5ba5603901c269426828fc72bd15482cdad988e8e192202dd931f6`. The test-only increment adds helpers inside existing Access and one THREADS_ENABLED testcase, [SceneTree][ProductionRequest][NativeSceneReleaseBudget].

The fixture uses the existing actual Registry, RenderingServer enrollment, original reserve and original cancel. Its default declared action is cancelled before recording, and assertions require native_count0, graph_count0 and maintenance_serial0. The privileged operation is explicitly labelled: injecting a real Ref<SceneReadContent> into an existing action's cleanup slot. validate always returns ERR_UNAUTHORIZED, scene_key_bound is checked false before and during every turn, and no permit, key, sample, physical context, native Complete or maintenance identity is created. This is a cleanup-leaf unit with real retained-reference ownership, not public Scene content acquisition.

## Actual original RELEASE and accounting

With the Ref retained, strict reclaim returns Busy. Readiness-only release returns OK without calling the virtual return function, and calls remains0. These are direct original helper observations, not fabricated request readiness.

Each of the three subsequent turns accepts an original RELEASE control with a fresh immutable authorization (controls1/passes1/receipts1 and a real deadline), schedules it through the original FIFO and syncs the actual server. The test requires RETURNED before reading its output. Expected behavior is:

| Original RT turn | Actual virtual return calls so far | Content destroyed | RT result | Charged controls/passes/receipts |
| --- | --- | --- | --- | --- |
| 1 | 1, returning Busy | 0 | Busy | 1/1/1 |
| 2 | 2, returning OK | 1 | Busy | 1/1/1 |
| 3 | 2, no content left | 1 | OK | 1/0/0 |

The successful content return deliberately still returns Busy for that bounded turn; a later original turn establishes release readiness. After every RT result, the test consumes the exact original control and proves owner consumption does not invoke return_after_reads or spend owner passes/receipts. Final original owner consumption reclaims the request and clears serial; original owner cleanup must finish. The test does not call the virtual return directly, drive a private alternate queue, or zero a request state to obtain this result.

This no-maintenance fixture validates the previously missing RT content-service authorization and its charges. It does not exercise subtracting already spent native-maintenance passes/receipts or merging nonzero maintenance usage. The README explicitly preserves that remaining boundary; native_count0/graph_count0/maintenance_serial0 make that limitation executable rather than merely narrative.

## Mutex probe, synchronization and lifetime

Each return callback and the actual content destructor calls check_unlocked. It starts a distinct worker thread that invokes Registry::mutex.try_lock, unlocks only on actual success, and exits. The RT waits_to_finish before reading acquired or reusing its storage. This avoids a recursive same-thread mutex false positive and supplies a join synchronization point for the worker write/main-thread read. The worker does not access the content Ref, request controls, calls or destroys; those counters are touched in the original callback/destructor execution lane. State storage remains alive for each joined worker.

The worker uses try_lock rather than a blocking lock, so root's controlled removal of the callback's surrounding unlock/lock should produce a failed acquired CHECK and return, rather than a cross-thread wait cycle. CHECK is nonfatal, allowing the original Busy→OK→reclaim sequence and actual Ref destruction to proceed during that counterfactual. The expected three successful checks on the normal source are two virtual return attempts plus one actual destructor. Because the original local returning Ref retains ownership until after content.unref, the test's destruction assertion tests last-reference release rather than a destructor flag assigned by the fixture.

Thread-start failure is observed by CHECK(worker.is_started()); no join is attempted unless the worker started, and acquired stays false so the lock check also fails. No probe thread can remain running after the normal check_unlocked returns.

Root incorporated the review's lifetime recommendation: SceneReleaseState now precedes SingletonEnrollment, so the fixture and its request-held Ref are destroyed before their callback-state storage. This removes the local state-lifetime ordering concern even on fatal assertion unwind. It does not claim that the existing fallback can replace original queued service of arbitrarily broken or still-Busy content; the successful path and intended nonfatal counterfactual use the explicit three-turn service sequence.

## Evidence attribution and limits

Root reports focused execution 1 case/102 assertions, exit0 before the declaration-order adjustment. Root then reports the controlled callback-lock counterfactual produced exactly three intended failures: each of the two callback worker try_locks failed, and unlocked_checks was1 rather than3; the other99 assertions and cleanup passed, with normal exit1. These results are attributed to root, not independently run or receipt-inspected by this reviewer.

Afterward this reviewer read the final state declaration order, confirmed the actual production callback unlock/lock and destructor unlock were restored, and observed a test-only diff of102 inserted lines. Restored production_request.cpp SHA256 is `84b602ceed1846e0a0ff9e02166779b578fd82ef8ddde6584b8fa40c9f78339f`. The restored-source rebuild was still running when this addendum was saved; no final execution result is inferred.

The test does not cover acquired Scene content, validation success, source/sample semantics, GPU reads, native completion, same-turn multiple-content traversal, existing-maintenance credit consumption, deadline exhaustion, or broader Scene readiness. Its evidence is specifically actual Ref cleanup under original RT RELEASE accounting and mutex separation.
