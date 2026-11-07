# P01 r3 independent review

Source/oracle review CLEAR at base `1e0a609539`. Frozen inputs reviewed: probe SHA256 `4eb6abe17f9ac83efcb1686507e739243296820a98fecc4ca62e5367e6f1bf59`; runner SHA256 `a7da6cd40534c7a6b630f96ae9d0a138bd86c70b2347fb35fdde8a61410ce4f8`. Both final oracle closures are present: exactly one successful kind9 operation equals the final acceptance, and every retry summary's fact count/revision must match its actual snapshot header.

The unchanged `qualification_record_prepared` reason19 paths precede original record identity installation/recording and `qualification_record_append`. They cover RD lock refusal, native/fact capacity reservation, preflight range refusal, recording/list/frame-wait/callback conflict, and prior-frame upload not yet successfully returned. RTObservation exposes only the common reason, not which one occurred. V1Report and revision-scoped complete facts distinguish new capture/body/readback (semantic1/2/3) from existing upload/native progress (6/4/5). Observing the latter may legitimately progress old work; no full-graph equality is claimed. The caller rejects new record facts and nonzero record-owned source/readback fields; it preserves exact request bytes and bridge/ticket identity through a finite retry window.

## Actual parent run independently revalidated

Read `native-artifacts/RESULTS.json`, original POSITIVE/CANCEL_PREFIX raw logs, and before/after input manifests. Extracted only the frozen runner's three pure validation functions through Python AST and called them offline; did not import or execute the runner's launch/build/body code. All118 occurrence check sets and terminal facts match saved RESULTS, with all checks true:

-117 positive occurrences,117 actual accept records,1521 terminal facts,4446 positive QRT return records.
-1 cancellation occurrence,3 terminal facts; original reason16 cancellation refusal retained.
-Before/after frozen input manifests equal.

**Actual positive retry count is0.** This run proves all original117 admissions/terminal controls with genuine pre-call no-record snapshots; it does not provide a real44/reason19 refusal followed by success. No actual reason19 sub-branch can be inferred. This limit must remain separate from source proof and oracle sensitivity.

## Actual-log negative sensitivity

Eight mutations of actual occurrence0 logs are all rejected by the frozen `qrt_original_returns` oracle: extra successful kind9 with complete snapshot, missing final accept, wrong accepted ticket, capture semantic in a snapshot, inconsistent snapshot fact count, replacing snapshots with a noRecordAppend boolean, busy19 without typed retry evidence, and changing cancellation16 to19. These are offline corrupted-log checks, not injected native behavior. The retry-specific header-summary mismatch is source-reviewed; no original retry exists in this run to mutate.

`RESULT.json` records counts, all eight outcomes, and hashes of frozen caller/oracle, RESULTS and both actual raw logs. `review.py` reproduces only offline validation. An initial reviewer-script comparison used Python tuples against saved JSON lists; it was corrected to compare list values before the successful validation, without changing frozen inputs or logs.

No production/source edits, build, GPU execution, staging or commit by this reviewer. Independent output exists only in this separate review directory. These are native substrate controls; no E06 Verified/Ready, connected Scene/E09, runtime authority or public qualification follows.
