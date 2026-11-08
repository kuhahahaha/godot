# Final independent test/source review

2026-11-08. Reviewed the current live seven-file diff, without editing source or running build/compile/tests. All seven live files match the candidate `after/` copies byte for byte. Candidate patch SHA256 is `5477b30ec7b6e415009075923eb0229edcf5b72f018c7eb85573ea68b612a73f`; current `git diff -- <seven paths>` SHA256 is `2ba454550b177af86a92c0ace82271387e4f345fc36f68bf18938738279f8913` (different patch formatting/index metadata, same after sources). Live test file SHA256 is `fb388eb54473e22dd6eef5cb728f697ea94f09d1c7e16c721db3cee9648a14d4`.

## Verdict

No new concrete source or fixture-control blocker found. The earlier V1-only reservation evidence gap is closed at source level: the revised test uses installed typed query and real V2 producer-shaped upload/incoming-readback metadata reserve calls. The explicit guard and RAII design correctly handles ordinary failed checks and explicit early returns under `-fno-exceptions`. Execution and broader native-profile acceptance remain outstanding; this review establishes neither test pass nor Runtime96 end-to-end closure.

## Failure control and cleanup

- `tests/core/extension/test_production_request.cpp:994–1036` obtains the existing SceneTree/server preconditions and installed interface pointers before use. `ordinal_server` reports failure through `check_step` and returns null; the caller returns immediately. Each installed pointer is explicitly checked before invocation.
- The local `ensure(bool,const char*)` receives each operation result once, logs it with `CHECK_MESSAGE`, then returns that same boolean. Every required setup/query/reserve/lookup result is followed by `if (!ensure(...)) return;`. A failed check therefore cannot continue into a missing procedure, failed quote, absent request, or null pointer.
- This matters because `tests/test_macros.h:44` enables doctest's no-exceptions assertion mode, and `thirdparty/doctest/doctest.h:4134` makes `throwException()` empty. `REQUIRE` cannot be relied on to unwind here. The new Requirements routine contains no such REQUIRE-dependent safety path.
- `SingletonEnrollment` is constructed before initialization, so its original destructor handles partial setup on explicit return. `defer_reserve=true` exits initialization with the accepted attachment and installed receiver/open state, before creating an implicit request. Thus no untracked setup request competes with the test's two explicit refs.
- The `Requests` guard is constructed after successful owner initialization and before the first reserve. Successful reserve writes its original ref directly into the guarded array. If producer reserve fails, both refs remain empty; if producer lookup/query/reader reserve fails later, the first ref remains guarded. Both acquired request pointers are explicitly checked before dereference.
- A return from inside a lookup's lexical `MutexLock` scope destroys that lock before unwinding the outer request guard. Cleanup therefore does not call Registry cancel/release while retaining that lookup lock. The request guard is destroyed before the enrollment fixture, so both refs receive original cancel then release before original attachment close.
- Cleanup uses nonfatal `CHECK` expressions whose operations are still evaluated under no-exceptions mode. Both refs are attempted even if one cleanup assertion fails. This is appropriate for these reserved, unprepared requests: there is no issued work, active control, imported use, or source borrow to drain. The guard does not promise recovery if cancel/release themselves encounter an independent invariant failure; such a failure is reported, and fixture close would also expose outstanding children. No fake retirement/refund was added.

## Typed double-request proof now present

At lines 1004–1006, procedure pointers are obtained with the generated exact typedefs from the installed interface registry. The test calls the requirements query, `device_session_production_reserve_v2`, and `device_session_production_reserve_readback_sources_v1` through those typed pointers.

The original accepted pool is fixed once at two requests/two actions/two resources/two uses/two graph memberships, native32, one readback, two returns, payload8/readback64. No subsequent pool growth or arbitrary quartering occurs. Producer quote consumes one graph/native/return and command4 with payload4. Its typed reservation is retained while the readback query executes. Reader quote consumes one graph/native/return and command4 with readback4 and payload0; its distinct original nonce prevents request-identity collision. The 33-byte quote intentionally tests the total pool bound without reserving that larger reader while the producer already occupies a graph slot.

The test looks up both actual Registry requests, asserts contract_version2, verifies the normalized RETAINED_INCOMING declaration/backing serial, two children of the same attachment, preserved first serial, separate graph/native/return pointers, and nonoverlapping command ranges. Because these particular graph/native/return slices each have length one, distinct pointers prove their disjointness. Fixture maxworkers0/currentprefix1 and the existing slot formula give four commands per request, fitting the accepted eight-command pool.

This is genuine typed metadata reservation evidence in the source, replacing the old two-V1-upload helper calls. It still does not import a physical buffer, authenticate the literal fixture backing_serial123 against a producer carrier, prepare/freeze/append a request, retain a producer-arena borrow, or establish GPU order. The test comment and applicability document now state that boundary accurately.

## Earlier gap status

| Earlier gap | Current source status |
|---|---|
| Installed typed query + actual V2 upload/readback reserve coexistence | Covered by revised test source; not executed by this reviewer. |
| COPY/DISPATCH/UPLOAD/READBACK sizing branches | All four now invoked. Readback33→graph/native2 and4→1, resource-indexed returns and descriptor-count addition are asserted. These are sizing branches, not actual copy/dispatch preparation. |
| Failure output unchanged | Binding-count overflow, unsupported native output, missing upload correlation, block16 and malformed readback correlation covered selectively. Full malformed-span/overflow/row-correlation matrix remains absent. |
| Resource ownership/readback tag validation boundary | ABI/README now expressly limit query validation to sizing-bearing fields. Original reserve/prepare remains responsible for structural/full closure validation. No source authority is inferred from success. |
| Nonzero workers, pending completion/native profile, current-prefix drift | Still absent. QuoteDriver uses maxworkers0 and native1; no profile or prefix mutation is tested. |
| Full disjoint arena assertions | Graph/native/return/command assertions present. No explicit frames/resources/uses/suboperations/payload/readback range assertions or full first-request snapshot compare. Ordinary allocator behavior is inspected, but the test does not claim comprehensive arena mutation coverage. |
| Link/build/test execution | Still required. Author-reported isolated syntax passes do not establish linked installed-port behavior or test success. |
| Actual Runtime96 frozen plan/applicability | Still missing. Actual ownership/action/source arrays are not present; OUTPUT_ALIAS/NATIVE_OUTPUT remains explicitly unsupported. |
| Producer source borrow, checked join, GPU execution | Still outside this fixture and unproved. |

The prior staging arithmetic/native-profile source conclusions remain unchanged: no new underquoting or authority issue found in the live implementation. Required next evidence is execution of these focused tests and targeted nonzero-worker/profile/prefix tests, followed separately by the original Runtime plan and source/order proof. None can be replaced by this static review.
