# Actual finalize/LASTREF child review — P1 closure

Scope: independent read-only review of `test_native_height_range.cpp`, `native_height_range_finalize/run_finalize_entry.py` and its README. The R2 follow-up reviews only the previously reported false-positive defect. The reviewer did not edit implementation, build, invoke the engine/GPU or commit.

## Original review

One P1 was found. Driver callbacks used nonfatal doctest `CHECK(serial == 71)`, while child `_Exit(0/78)` bypassed doctest's final exit status. The original runner accepted expected exit plus positive markers without rejecting failed checks. Pure replay of its predicate accepted a failed CHECK diagnostic with the normal busy or LASTREF markers. This allowed a wrong-serial callback to fail its check while still producing a successful child result.

Original reviewed pins:

- CPU test: `d6545f305e3600946690cf11915fdcf57f3e33b30fc13b10f16e513a2ce39d3b`.
- Runner: `f1b2023fbbcaaa8d147f40097e781e3f9a0356a57e52f851ed37dd08bd38f061`.
- README: `a48568ede106dfe57df1f024b2377026c0b8b87856cce9a9f881e4bbdf636a58`.

The remaining requested properties were source-verified: actual public `rd->finalize()` is called twice; the test does not set the finalization phase or cached native completion; the heap RD is passed to actual `memdelete`, whose destructor retries finalize then requires LAST_REF success and exits 78 on BUSY. `command_pool_free` is the first driver teardown call for this empty Frame and its sentinel exits 87 before fake/null teardown can proceed. A returning destructor exits 88. The default test reads the environment but performs destructive work only for exact `busy`/`last_ref` modes. Constructing this RD does not replace an existing singleton.

## R2 result: P1 closed

All four relevant child-mode driver callbacks now use shared fatal `child_require(serial == 71)`, which prints `NHR_FINALIZE_ASSERTION_FAILED` and exits 89 immediately. Ordinary CPU cases retain their original doctest checks. Actual-child invariants use that same fatal helper. A wrong serial can no longer reach a successful marker/exit by bypassing doctest's summary.

The runner now applies one common rejection predicate to GREEN and controlled RED modes. It rejects the fatal child marker, failed doctest checks, fatal errors and nonzero failed summaries. Source inspection and a pure regex check extracted from the final formatted runner reject seven representative bad diagnostics and retain the expected finalize-BUSY error, normal retry marker, LASTREF marker and controlled sentinel. See `PREDICATE_CHECK_R2.json`; this is parser evidence, not an engine execution.

Final pins are in `FINAL_PINS_R2.json`. The CPU test is `5f69c203a88f8b5ca3ae66e117b79ed98044480c6017b07275a95edb1d581b3b`; the root-formatted runner is `1d6b71d80f2abf774dfc021e38ce17391347fa63077c6829a4bafdb4835bff6b`. Formatting did not alter the reviewed predicate. Restored production RD is `69c3d7bf81ebf5ebd92692d1423805248b9971e9f829d1b19f2b22d633dd10fc`.

Existing root-generated R2 result receipts were inspected: GREEN busy exits 0, LASTREF exits 78; removing only the production `height_return` refusal guard reaches sentinel exit 87, the controlled-RED runner accepts it, and the normal runner rejects both modes. The restored child receipts again report 0/78. Final formatted-runner re-execution remains root-owned; this review does not claim the reviewer ran it.

No unresolved P1 remains within this narrow follow-up. Scope remains synthetic-source CPU lifecycle evidence of actual entries; it does not establish real Metal teardown, full Scene ownership or a public ABI producer. As the README states, an idle Frame's stable counters cannot independently prove that an empty second flush was skipped; the test also checks the real phase flag and repeated safe refusal.
