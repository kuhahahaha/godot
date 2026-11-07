# Independent review: returned alias readback map failure

Verdict: no source blocker found in the reviewed two-file diff. The production change narrowly admits the request's original owned maintenance continuation after an actual returned CPU map failure. It does not make the failed readback CPU-ready or relax final reclaim.

## Reviewed identity and scope

- `core/extension/gdextension_production_request.cpp`: SHA256 `de29fcb1312b928865f9c93ad28eff4d8ba4edd76599dd7f301989c07bdfcd1b`.
- `tests/core/extension/test_production_request.cpp`: SHA256 `275231b761d8288308adef9502e5949c3cbceb20329b9927adb4b23c205eaff0`.
- Independent source review used the active checkout diff and surrounding release/report/frame paths. This reviewer did not build or run tests; root owns the build/test lane.

## Admission and custody

The exception requires the exact `ERR_CANT_ACQUIRE_RESOURCE` report cause, no possible obligations or pending physical returns, all known obligations complete, no effects pending or device loss, original nonpending graphs without issue errors, and validated retained native owners with KnownAccepted issue, Complete observation, and no source/completion/retention/release error. Each readback must belong to the request, have returned all issued bytes, and have no impossible completed extent. Every readback error must be the exact map error with a nonzero issue and no delivered result; at least one such error is required.

The existing outer conditions still require ERR_BUSY from strict release, the original request-owned active maintenance serial, no capture or open list, and exhausted or cancelled actions. `production_release_locked` and public copy readiness remain strict. The original frame map path creates the tested failure and returns staging custody without creating completed/delivered CPU bytes.

## Test review

The bounded driver fails only the actual download buffer map, reports native code -7001, and delegates unrelated maps with the original default native-code behavior. The test verifies one attempted/failed map, issued4/returned4/completed0, no delivered bytes, no outstanding capture, empty original frame download requests, zero ready readbacks, and unchanged destination/written sentinels on repeated public copy errors. No readiness or completion is seeded for this failure.

Eight reversible refusal contrasts exercise Pending and Unknown native observations, a completion error, incomplete physical return, a different readback error, a pending graph, foreign/missing maintenance ownership, and native IssuedUnknown with an otherwise Complete observation. The eighth contrast changes and restores the issue fact for every owner/alias member sharing owner_index0, preserving source equality so that KnownAccepted admission is independently exercised. Checking OWNER_CONSUMED after each refusal matters: an admitted RELEASE publishes its original control cell and returns Busy before later consumption, so these checks detect refusal of publication rather than merely a Busy result.

The actual cancel/slot drain/generation withdrawal/native retirement and request release path is retained. During each release turn while the original request is live, the test checks retained readback error28, returned4/completed0 and !delivered; a successful final release checks report cause28. This correctly distinguishes retained result failure from a bounded control report: the existing consume path replaces report cause with the completed control's ERR_BUSY when that turn is unfinished. The earlier assertion that every public release report must retain cause28 was overstrong; its failed receipts are not evidence of a production defect.

Final cleanup still requires original request serials cleared, all eight actual allocations freed, staging returned, CPU owners freed, and singleton restoration. This is bounded CPU/driver-fixture evidence, not GPU qualification or broader scene readiness.

## Execution evidence attribution

Root reports final r3 focused coverage of 2 cases / 1664 assertions, exit0 with no diagnostics, and combined coverage of 218 cases / 93718 assertions, exit0. These final execution counts are attributed to root and were not independently run by this reviewer. Earlier r1 receipts were independently inspected (2/1606 focused; 218/93660 combined), but predate the eighth contrast and corrected per-turn assertions. Earlier r2 failures of the overstrong public report assertion remain historical failed evidence, not a passing final run.
