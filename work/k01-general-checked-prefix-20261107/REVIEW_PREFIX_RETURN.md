# Independent review: failed-prefix observer return

Reviewer: depth_quote_independent_sol (default Sol). Date: 2026-11-08.
Base HEAD: `5ca094c9ece554ec212708695e48004c8d75b53e`.
Scope: current uncommitted changes in `native_height_range.h`, `rendering_device.cpp`, and `test_native_height_range.cpp`; read-only source review, no build or test run. Geometry owner/sink integration is outside this slice.

## Findings and disposition

### P1 initial finalization swallowed sticky physical return errors — CLOSED in revised source

The first version allowed any observe error when cached `prefix_settled` was true. After a non-BUSY `native_call_release` failure, the next finalization could therefore ignore sticky `return_error`, retry the original release and drop the range if the retry returned OK. This was broader than the stated business-error exception.

Current `rendering_device.cpp:10587-10588` rejects sticky `return_error` before retrying release. Current `10649-10653` permits the finalizer exception only when `append_error != OK` and mapping/issue/retention/completion/return errors are all OK. Consequently the physical-failure path keeps ownership and cannot replay or manufacture successful finalization. BUSY remains retryable because it does not set sticky `return_error`.

### P2 test error precedence and negative coverage — CLOSED

The final test keeps the established error precedence: the failed observer-return case expects `ERR_CANT_CREATE` from the append error and separately asserts `snapshot.return_error == FAILED`. The sticky-return guard therefore remains observable without changing the existing append-first error contract.

The final table-driven `business_failure_does_not_settle_unsafe_prefix` case covers mapping, issue, retention, completion, uncaptured, unresolved issue, and wrong completion serial. Every row asserts no `prefix_settled`, no `native_complete`, no return, failed finalization, preserved enrollment in both RD and frame lists, and zero releases. This closes the previously suggested unknown/physical-error negative gap for this reducer slice.

Controlled RED evidence is meaningful and source-restored: `business_gate` removed the finalizer business-error gate and built successfully, then the focused case failed 4 assertions; `sticky_release` removed the sticky `return_error` guard and built successfully, then the focused case failed 6 assertions including replay/removal. Both RESULT files report `build_exit: 0`, `test_exit: 1`, and source restoration to RD SHA `d1dcc010516b005fdf8e2847855512666032ed8d2060097bca0613d3c9332ea5`.

## Physical prefix gate

`prefix_clean` at `rendering_device.cpp:10538-10542` requires closed recording, a real recorded effect, nonzero graph/source counts, every graph sealed and issue-attempted, every source retained and completed, all physical error fields OK, and no uncaptured prefix. Unlike `native_complete`, it intentionally ignores append/business error. Business success remains false when append_error is sticky.

Unknown handling is conservative: incomplete or mismatched source completion cannot set source.complete; IssuedUnknown must be authoritatively resolved to KnownAccepted by the original poll. An unknown issue that is never resolved, an uncaptured prefix, mapping/issue/retention errors, and a native completion error do not establish prefix_settled. A known accepted/completed original source that was formerly IssuedUnknown may settle after authoritative resolution; this is an existing source-observation rule, not an unknown-as-complete shortcut.

The cache is safe under the current owner lifecycle: recording_closed prevents new record work; every graph was already issue-attempted; retained/completed counts cannot gain new sources through replay of those members. The late physical failure that can occur after settlement is observer return, now separately sticky and explicitly gated. A future general geometry append/continuation owner must preserve this closure property; this audit does not certify that missing integration.

## Coverage and exact negative-control suggestions

The two added cases exercise the intended synthetic reducer behavior: (1) fully settled physical prefix with sticky business error returns its source while native_complete stays false; (2) failed observer return remains sticky and is not replayed. They exercise `_native_height_ranges_prepare_finalization`, a mechanical helper. They do not exercise actual public `finalize()` or genuine failed geometry append.

Existing failed-source and known-no-effect/uncaptured-prefix cases plus the final seven-row physical-error table provide the requested narrow negative coverage. Direct private-state injection is explicitly synthetic; real caller integration needs its own future fixture. The table is source-reviewed, with root's final green run pending at the time of this report update.

The original RED recipes below were superseded by the root's actual narrower mutations: business_gate changes `if (observed != OK && !sticky_business_error)` to `if (observed != OK)`, and sticky_release removes only the early sticky return_error guard. Raw XML proves the first mutation fails finalization OK, empty enrollment, observers_returned and release-count assertions (11 successful/4 failed assertions). The second fails six assertions: retried give_back is incorrectly OK, release count increases to two, finalization is incorrectly OK, observers_returned becomes true and enrollment disappears (14 successful/6 failed assertions). These are direct behavioral failures in the named target cases, not compile errors or crashes. Each XML has exactly one executed failing case; unrelated cases are skipped by the focused selector.

## Reviewed source pins

- native_height_range.h SHA256 `7ec8a6fcb70963ac8d61c6ed4344d0213263df40f35bb9f49e2b5a63e77fbbf9`
- rendering_device.cpp SHA256 `d1dcc010516b005fdf8e2847855512666032ed8d2060097bca0613d3c9332ea5`
- test_native_height_range.cpp SHA256 `33cfad2d25887b5ecb59f85b8d8da1bdc68c5b6ff01515a8d85c6e413c9f92d9`

Controlled RED results were independently inspected from the recorded RESULT/CPU.xml artifacts; no source edits or build were performed by this reviewer. Root owns final build/CPU evidence and source changes after these pins.

Controlled RED artifact hashes:
- business_gate RESULT `95d5dd0f6508ea71bf9cdaa7c6a00bc5347c038eebf31ca3134321ba573d1890` (recorded source-restored result; CPU.xml SHA256 `b17ad17ca0d3c19160e59a46debc1dfbc2a16c85d8a981c30e4bbf09161fdff3`)
- sticky_release CPU.xml SHA256 `a8c649c20bfdf02ee11b430b09b005a605a8c9a67143c39e5150c465b5c03635`
