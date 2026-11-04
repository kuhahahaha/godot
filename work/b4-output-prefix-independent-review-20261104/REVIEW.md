# Independent Instance192 allocation-prefix review

Result: **no scoped source finding** in the five final files identified by `REVIEWED_SOURCE_SNAPSHOT.json`. The final header/source/test hashes match the author's frozen snapshot. This review edits no author code and starts no SCons build or host. Seven new engine tests remain **unexecuted**; root owns the next build/runtime gate.

Formal references read: the unique `FR_B4_external_output_PLAN.md` P2/P4 and `contracts/b4-native-output/OUTPUT_CONTRACT.md` acquire/zero-capacity/error/retirement rules, plus the existing Core NativeDisposalWitness and `_native_disposal_*` implementations. The reviewed class is a mechanical owned-buffer prefix only. It does not implement those full P2/P4 joins or the public six-method API.

## Source findings and boundary assessment

All owned extents are checked before the first native allocation. The validated envelope bounds N to2^20 and surfaces to1..8. Current/previous lengths are192*N each; metadata32/scratch64/status32/commands20*S/proof224+144*S match the existing guard layout. The u64 sum is planned ownership bytes, **not** a budget grant or charge receipt. Actual native allocation limits/errors remain with Core; the current RD Limit enum exposes no maximum storage-buffer-byte query. The original owner must admit the real budget before using this internal component.

Capacity0 omits both record allocations, leaving the five actual control/proof allocations. No dummy record, caller-supplied source RID or source descriptor is accepted. The protected commands request the existing Core indirect-usage bit. No uniform set, initial upload, shader, packet, issuer, token, source registration or PublicSuccess result is produced.

The single-use guard is closed before the allocation loop, so a later creation failure preserves every successful original RID/witness/extent, preserves the original creation error, requests normal Core disposal and cannot reopen or overwrite that prefix. Invalid input/native preflight refusal occurs before the attempt flag/resources change; such an unattempted object can still be used by its valid owner later. The private AlreadyInUse/invalid-input checks precede Core allocation preflight; they cause no native effects and are not claimed as the future public RS error-mapping policy.

Retirement rejects a foreign owner/wrong RT before changing retirement_requested, including an empty prefix. On the correct owner/RT, active-list Busy closes future allocation logically and retains the native entries until later explicit retries; it does not convert LIVE to UNKNOWN or RETURNED. Each Core free is individually attempted and failures retained; observation errors take precedence over the first request error. Creation_error remains independently available in Report. Repeated retirement does not allocate or append a new prefix.

Observation is allowed during active lists, as in the original Core recovery observation port. Every nonnull created RID contributes to created_count and gets an observation attempt; a refused observation leaves its per-resource state EMPTY and propagates an error, so the counts may be incomplete but **cannot** authorize destruction. all_driver_returned requires no sticky UNKNOWN, no observation error and returned_count==created_count. An ENQUEUED entry never qualifies. A prior UNKNOWN remains aggregate-sticky even if an artificial mock later reports every cell RETURNED. Empty-created prefixes are vacuously returned only for deleting that empty component, not a whole-output release.

Only controlled destroy checks these observations before deletion. Wrong-thread/observation errors, LIVE, ENQUEUED and any prior UNKNOWN keep the pointer. The destructor submits no work and guesses no free completion. maintenance delegates once to the existing Core method; it creates no scheduler or completion clock. Successful actual frame-maintenance behavior is not established by source or mock invocation.

Construction/destruction/copy and methods are private to ParticlesStorage plus a TESTS-only access friend. The only outer-header changes are two TESTS_ENABLED friend declarations; there is no new production Core port. The single future ParticlesStorage owner still must enforce recorded-use closure, hold the object and real budget charge, and retain its original K03 device lease. The stored RD reference is not that lease. Those source/operation/material/history/device joins are absent and cannot be inferred from this component.

## Independent execution

`run_independent.py` compiles the verbatim prefix class/function bodies and the existing checked-byte function with explicitly **mock RD/witness dependencies**. It supplements the author's port with preflight/error injection only; it is neither real Core queue execution nor GPU proof.

Both normal and ASan+UBSan runs exit0, with no sanitizer diagnostic. The independent controls check:

- All16384 LIVE/ENQUEUED/RETURNED/UNKNOWN combinations across seven resources: exact full counters, error precedence, and destruction allowed only for seven actual RETURNED observations.
- A failed observation among otherwise returned resources: created7/returned6, error propagated, no destruction; a subsequent valid observation allows completion.
- Wrong RT leaves retirement state/report output untouched; three active-list Busy retries retain every LIVE entry, close allocation once and never promote completion.
- Allocation becomes Busy after two successful creates: exact two-resource prefix and original Busy error survive; retry retirement closes those entries without reopening allocation.
- Sticky UNKNOWN remains a destruction blocker even after mock-only state repair produces five returned observations and unknown_count0.

Raw logs/commands/source hashes are in `INDEPENDENT_RESULTS.json`. Mock test teardown deliberately deletes retained objects only to release the test model; it is not a production UNKNOWN reset or proposed lifecycle path.

The author's frozen source-body production and three targeted mutants were separately recompiled/replayed in this review directory. Production exits0; dropping failure-prefix retirement, treating ENQUEUED as returned and dropping sticky UNKNOWN each fail the intended exact assertion. Commands, candidate hashes and diagnostic matches are in `AUTHOR_CONTROL_REPLAY.json` and `author-control-replay/`. The author package was not regenerated or changed.

All three author syntax commands were independently rerun: production component TESTS on, test TU and production TESTS off each exit0. Commands/log hashes are in `SYNTAX_REPLAY.json`. These are syntax checks, not engine-test executions.

## Remaining acceptance

No new source finding remains in this component. The seven actual Core-queue/fake-driver engine tests still need their built-host run, including process shutdown checks. Their manual ready queue drain will not prove successful real no-next-draw maintenance or GPU retirement. Original use/budget/source/K03 ownership, actual device faults, native disposal, guard recording, renderer adoption, owner ACK and whole OutputReleased remain separate integration/runtime obligations. No broad B4/P2/P4/ANGE completion is signed here.
