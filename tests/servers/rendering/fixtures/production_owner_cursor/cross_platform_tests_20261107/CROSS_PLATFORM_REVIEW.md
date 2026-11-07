# Cross-platform test-only CI repair review

2026-11-07. Independent review against HEAD `acd6310a0b84ff80ea90cec106f4a0e9ec7a855c`. CLEAR for the two reviewed test-TU repairs. No source edits, builds, GPU runs, commits or remote polling by this review. Concurrent Sprite production changes are outside this CI review.

## RecordSource: exact formatter identity and branch binding

File: `tests/servers/rendering/test_record_source.cpp` (the earlier core/extension shorthand is not its actual path).

Reconstructed the previously reviewed three-line narrow patch from HEAD and verified its complete source SHA256 equals `c430912186d9b7300a8b420b9276941e385fa7e8453d2a7399cadb2bdf8eb189`. The patch adds `Semaphore entered{}, release{}` to Contention and PoolHolder and renames only PoolHolder's lambda-local state to probe. It leaves the aggregate pointer member, lock/post/wait/unlock order, outer state references, test assertions, budgets, source facts and cleanup unchanged.

Executed the actual local formatter `work/ci-scene117-20261107/tools-venv/bin/clang-format --version`:21.1.7. Fed that reconstructed snapshot to this formatter on stdin with the actual source filename. Its output is **byte-identical to the current entire TU**. No in-place formatter was run by this reviewer.

Independently compared lexical tokens after retaining string/character literals and removing comments/whitespace. Relative to the narrow snapshot, only the two include positions swap (semaphore.h before thread.h), and four single-statement if bodies gain brace pairs. No other tokens are inserted/deleted/replaced. Include multiplicity is unchanged. Manually checked all four added pairs:

1. failed `claim_native_fact(fact_index)` encloses only `return ERR_PARAMETER_RANGE_ERROR`; calls/source recording stays outside;
2. `if (worker)` encloses only `memdelete(worker)` in WorkerCleanup; pool pointer/counter reset stays before the condition;
3. `segment_calls > 1` encloses only `return OK`; first-segment source/report construction stays outside;
4. `error != OK` encloses only `return error`; subsequent calls/report construction stays outside.

None has an else clause or changes an enclosing branch binding. Remaining657insertions/389deletions versus HEAD are formatter whitespace/line splitting, the include swap, four brace pairs and the explicit three-line repair above. Current formatted source SHA256: `9ea86fa137d95620d7d055013676b206210d3d9de32513642ba8843ecd24e1a5`.

Original remote diagnostics are actual acd run42366982660: Linux Minimal job128387119461 reports missing Contention entered/release at old255, missing PoolHolder entered/release at old420 and lambda state shadow at old421; TSan128387119483 reports missing entered at255/420. These fixes preserve warning rules and are not a production semantic change.

## RequestTU: exact initializer patch, separately scoped Sprite checks

File: `tests/core/extension/test_production_request.cpp`. Reviewed original CI-only patch `work/sprite-preparation-rows-20261107/android-contention-initializers.patch`:2389B SHA256 `7ef0f2b659d404e20fc1ca6967edc352639ee2990a700713edf5322d881141eb`.

Its entire intended delta is four explicit default member initializers (std::mutex, std::condition_variable, and two Semaphore ready fields) and two block-local RenderingDevice pointer renames device→actual_device with the corresponding reference passed to the same original Registry call. The default constructors remain the same; no initializer argument or synchronization protocol changes. The two pointers still initialize nullptr in the same scope, preserve lifetime/address semantics and call the same functions with unchanged other arguments. Checks and refusal/error handling remain unchanged.

Current shared RequestTU additionally contains two Sprite `preparation_rows_match_slots` CHECKs. They are separately authorized Sprite coverage, not CI repairs and not evidence of a wider initializer patch. Removed exactly those two lines in memory, applied precisely the reviewed initializer/name substitutions to HEAD in memory, and proved the resulting strings equal. No other RequestTU difference was concealed in this review.

Actual acd Android/macOS/iOS/Web64/clang-cl diagnostics identify missing mutex/ready initialization at old1026/7753/12324. Windows MSVC Editor128387119430 and Template128387119378 independently identify C4458 device hides class member at old12834/12859, treated as error C2220. The local rename directly addresses that distinct compiler diagnostic; it is not mislabeled as the same missing-initializer failure.

## Validation boundary

Source reconstruction, token/branch review, exact clang-format21 output comparison and scoped git diff --check are review evidence. Parent reported strict syntax RED reproduction, GREEN two-TU syntax and a unified build, then a subsequent final-identity rebuild after unrelated owner include normalization; those executions remain the parent's evidence, not runs performed here. No new GHA GREEN, test runtime, GPU success or complete platform acceptance is asserted. The existing Static Checks success for acd is separate from its real platform compilation failures.
