# Public5 R1 independent review

Reviewer: height_native_bridge_floor_sol, default Sol. Scope: the ten files in implementation/SOURCE_FREEZE_R1.json, original diff and new files, against ATTEMPT_AND_CLOSE_CONTRACT_R2.md, PORT_PLAN.md, ABI_CONSTANTS.md and implementation dependency/test matrices. All ten SHA-256 values matched the freeze when checked during this review. This is a read-only source review: no build, CPU/GPU execution, RED mutation or production edit was performed. Root owns execution. K01 height tests are outside this review.

## Finding requiring correction

**[P1] First direct Quote/Create bypass the original new-work admission gates.**

Frozen `core/extension/gdextension_qualification_scene_depth.cpp:89` and `:205` resolve the original Bridge with `admit=false`. After exact-retry/origin handling, the first Quote at `:128` and first Create at `:229` check only closing/quiescing/close_started/native-finalization. Consequently they never apply `Registry::qualification_bridge_locked`'s `is_production_metadata_entry()` rejection (`gdextension_qualification.cpp:67`) or its `guard_enrollment != QUALIFIED` rejection (`:93`). ATTEMPT_AND_CLOSE_CONTRACT_R2.md:45 expressly pins those original Bridge admission gates.

A concrete source-level counterexample is a valid same-render-thread Bridge reached while an enclosing RegistrationContext has phase PRODUCTION_METADATA. `is_production_metadata_entry()` is explicitly dynamic and survives nested ordinary callbacks (`gdextension_production_window.cpp:1258-1270`). First Quote1 nevertheless reserves a child/history/serial/private references and invokes the actual private RD quote; first Create2 on an existing successful quote likewise mutates create_attempt/state/revision. The same omission also fails to enforce the qualified-guard precondition. Neither case is an exact retry or cleanup operation. QRT21/22 reserve already performs canonical admission, but that does not protect the separately registered direct procedures, and dispatch cannot retroactively supply the missing direct gate.

Recommended fix: preserve the initial non-admitting authentication and immutable exact retries, but use the canonical admitting resolver before the first transaction/mutation, with accepted kind21/22. This must preserve original accepted copied work after close and Retire/observe/release cleanup. Add negative controls proving metadata/cleanup and nonqualified guard refusal cause zero output, no child/history/ref/revision changes, and no native quote. Do not make already-published exact retries depend on a new-work gate by changing only the initial resolver.

This finding was sent to root during review; root reports the author is correcting it and adding controls. It remains a finding against frozen R1, not a claim about an unreviewed successor.

## Remaining implementation review

No additional concrete implementation defect was found in the reviewed no-adapter slice.

| Area | Source conclusion and limit |
| --- | --- |
| ABI | JSON and generated header declare the same five leaves and fixed POD types. Explicit layout/offset/standard-layout/trivial-copy assertions cover every new wire struct; registration checks all five exact procedure typedefs. QRT extents include 21/22/23 and fit existing maximum buffers. Compilation/generator verification belongs to root. |
| Origin/retry | The retained Bridge child authenticates registry instance, child serial and full original slot/part/generation. Quote matches complete request bytes, including descriptor and Quote1. Foreign Bridge on the same RD cannot resolve it. Released origins remain in finite original history and cannot be reissued. Create2 and Retire3 store their own exact requests; immutable Create result replay is deliberate even after later retirement. |
| Quote | A first admitted transaction reserves finite capacity/history/cleanup/reference/serial state before calling original private RD quote outside the Registry lock. The child retains its original NativeDepthAllocationQuote. Both success and native quote failure publish one same facet with API OK plus sticky quote_error; no old generic error-output rule is relaxed. |
| Reservations | Successful FB owner reservation is copied from the native original Known(0); physical FB backing and opaque overhead remain Unknown. Unknown byte observations are normalized to bytes0 with knowledge0, preserving the distinction. |
| Missing owner | Create2 returns typed ERR_UNAVAILABLE/state8/NotCreated on the exact existing facet, preserves the successful original quote and allocates no replacement native pair. No real Scene owner/producer/native-prefix completion is claimed or demanded by this review. |
| No-native retirement | Quote-only, failed quote and adapter-unavailable Create can Retire3 to state9 with zero known-created mask, EMPTY child states and false aggregate. The Create-stage error remains sticky; quote failure is not rewritten as creation_error. Create after quote-only retirement is refused. |
| QRT pinning | QRT22/23 reserve resolves and pins the exact existing child before activation; QRT21 acquires the child pin inside its quote leaf, including exact replay. Pins remain through occupied whole-result delivery. Dispatch revalidates the child and accepted pin; facet release refuses while any accepted pin or matching occupied RT cell exists. Whole-result ACK drops the original pin. |
| QRT cancellation | Inactive/failed-delivery QRT23 release restores the original cleanup serial reservation when cleanup was not delivered. Successful delivered retirement consumes it; direct facet ACK can then finish without a second cleanup transaction. Shared cleanup slot and queue credits include kind23 and are retained while a Scene child is unacknowledged. This is source-traced, not execution-proven by the new five tests. |
| Close/collection | New child participates in original Bridge children/private refs and cleanup-credit retention. Collection requires acknowledgment, no quoting, no RT refs, no cleanup serial reservation and no matching occupied RT cell; it archives the origin before deletion. Close cannot delete an unacknowledged Scene child. Original accepted-kind proof authorizes copied work after close. |
| Kinds1..20 | No blanket admitted-error/output exception was added. Existing dispatch postconditions and old extents remain. All legacy regression execution remains root-owned. |
| Native isolation | Production RD change is only the friend declaration for this CPU test fixture. The new Core facet never creates or frees native depth/FB resources. Native Scene integration remains an explicit dependency rather than fabricated production wiring. |

## Validation gaps in the frozen five tests

The five registered tests cover actual Registry quote leaves, successful and failed quote retry/archive, unavailable Create, foreign origin, malformed overlap, private-reference/serial refusal, and one copied 21→22→23 delivery sequence. They do not establish every claim in implementation/TEST_MATRIX.md:

- Missing finite CHILD_CAPACITY and complete history exhaustion controls. Current capacity case tests private_limit and already-exhausted serial source only. Add original finite-boundary acceptance plus next-origin refusal, ensuring zero extra quote and no lost child/cleanup state.
- Missing inactive QRT22/23 cancellation and failed-delivery QRT23 cleanup-reservation restoration. These are new cross-cutting lifecycle branches; source looks consistent but deserves direct controls.
- Missing occupied QRT21 delivery close/ACK case, release attempt while QRT22 is merely reserved, genuinely new Create refusal after close, and actual `qualification_close` BUSY/drain/complete sequence. Current test sets `bridge->closing=true`, proves accepted Create and cleanup Retire delivery, and proves facet release BUSY at the Retire delivery stage only.
- The fixture's `dispatch()` explicitly sets private active/queued state and enrolls the queue pin before invoking actual dispatch. That is useful leaf/delivery coverage, but it does not prove original public activation/scheduling/queue enrollment. Keep the claim bounded accordingly.
- Malformed-pointer coverage currently checks overlap and invalid logical Quote attempt, not actual misalignment, invalid ABI, or each new Create/Retire/observe valid-output zeroing path. Cross-session/epoch/device/thread rejection comes from the existing resolver; the new tests directly exercise foreign Bridge/registry/generation only.
- Operation serials41/42/43 demonstrate that transport serials are separate from logical1/2/3. They do not demonstrate a mixed Buffer/Scene sequence sharing the original bridge-wide monotonic stream. Legacy baseline tests plus a mixed-family control would strengthen that claim.

These are missing validation, not additional demonstrated production defects. In particular, the missing same-Ref native owner, real partial-prefix retirement and producer interop cannot be truthfully tested by inventing an adapter in this slice.

## Fixture singleton check requested by root

No concrete singleton-restoration defect was found. `previous` initializes after `device` construction. If original singleton is null, RD construction sets it to `&device`, so `previous` is that pointer; fixture body restores it and the subsequent actual RD destructor clears it because singleton==this. If a previous RD already exists, construction leaves it unchanged, `previous` captures it, teardown restores it, and the test RD destructor does not clear it. `device.is_main_instance=false` and detaching driver/frames do not change that final pointer logic. An explicit before/after-scope assertion could document both cases, but this initialization order alone is not a finding.

## Verdict

Frozen R1 needs the admission correction above. The remainder is coherent for the explicitly scoped authenticated original private quote / typed no-adapter failure / no-native closure slice, with the execution and lifecycle coverage limits listed above. Re-review the focused fix and added controls on the next freeze before treating the P1 as resolved.
