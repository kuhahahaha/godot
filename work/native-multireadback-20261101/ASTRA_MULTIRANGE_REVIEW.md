# Independent V2 multi-readback candidate review

2026-11-01. Reviewer: `/root/astra_multirange_review`. Source-only review of the candidate, not a build or runtime receipt. No production file, shared build artifact, or candidate source was modified by this reviewer.

## Result

The initial candidate review found no remaining definite defect at that snapshot. A later independent root review correctly identified a Store logical-owner/native-allocator mismatch missed by that review. The Store delta review below supersedes the initial Store ownership assessment. Neither review is final native acceptance; fingerprints identify the reviewed source.

Authoritative wire: `work/native-callable-handoff-20261101/MULTI_READBACK_ABI.md`, RecordV2 size 448, ReportV2 size 328, RangeV1 size 48, ObservationV1 size 56. The superseded 440-byte draft is not the reviewed ABI.

## Findings addressed during review

1. The common V1 upload entry rejected all V2-mode tickets. The mode guard was removed from upload while retained for V1 record/observe.
2. Several old drain/issue/poll/maintenance paths overwrote the first ticket native error. Ticket-level cause assignments now preserve the first error; per-source facts still retain their own failures.
3. Readback sources were admitted independently of the original prepared upload plan. Each range now requires same-ticket role, RID, backing and exact span. Method controls require the original E05 allocator. The initial Store predicate incorrectly required an E24 allocator plan; root later identified and the implementation owner corrected that mismatch. METHOD_RESOURCE denotes the logical E24 destination, while the actual current native allocation is E05 with its original backing reference and native allocation generation. See the Store delta review below.
4. Method epilogue size was unconstrained. Shared V2 validation now requires precisely one u32.
5. Multi-range loops checked cancellation only once. Per-range admission now checks the atomic closed state under the registry lock; continuation also marks its current bounded range as recording.
6. V1 continuation admission retained legacy behavior while newly added seal-time checks imposed V2 Body ordering. New readback-copy ordering checks are now limited to V2 tickets, keeping both sides consistent for V1.
7. Cross-frame Body success originally scanned all ticket native facts. Newly joined hidden native work in a later copy frame could then invalidate a completed Body during sealing. The proof now checks the original Body graph's frame/epoch and exact command slot plus hidden sources, and requires an actual slot mapping.

## Reviewed properties

- V1 JSON types and function interfaces are unchanged except appended upload-role enum values; verified by structured JSON comparison against production. V2 symbols are registered separately. QRT kinds 1–9 retain their type sizes; kinds 10/11 use 448/328 and 152/344 respectively.
- V2 prepare checks serial capacity before taking ticket plus three result serials. Prepared capacity reserves the maximum two extra split tails; record uses the sum of per-range ceil(size/32), not ceil(total/32).
- Capture and Body append once. Each used range owns a separate real BufferGetDataResult with its reserved request serial and copy cursor. Continued work only appends unfinished suffixes.
- Two uniform sets per phase use exact pipeline format count, exact set index and format; both sets' dependencies enter the original owner checks and pin set. Typed texture/sampler identities are checked against original bridge children.
- Same-graph copies must have actual Body dependency paths. Capture-to-Body synchronization makes Body the synchronization source for subsequent graph commands. Later-frame admission and sealing require the original returned Body's successful native proof.
- Success publication waits for every real result to be delivered, error-free and exact-sized, along with native/graph return conditions. Copy validates every result before copying actual bytes at its admitted offset. No CPU sentinels or fabricated result data were introduced.
- Cancellation/failure seals admission and retains the emitted prefix; release still requires physical retirement, no live frame/recording/accepted-RT ownership, and valid reverse edges. Additional real result references are released together.

## Verification limits and required runtime coverage

This reviewer ran source reads, diff inspection and the structured JSON compatibility check only. No scons, doctest runner, GPU program, or production mutation was performed. The implementation owner reports generated-C-header plus full ABI assertion syntax checking; that report is not an independent build result here.

The builder still needs one-range V1/V2 Numeric regression, the actual Method families including Store destination, multi-frame staging exhaustion, cancel/failure after an emitted range, preserved first error/native code, malformed/foreign/short range rejection, explicit two-set and typed-texture ownership rejection, and the exact rebuilt engine/ANGE Metal pair. Include a later copy frame that already has a pending hidden native source to exercise finding 7. Static checks do not establish any native completion or Method qualification acceptance.

## Candidate fingerprints

| File under candidate/core/extension | SHA-256 |
| --- | --- |
| `gdextension_interface.gen.h` | `971a1ab90541900c50cde6eb45f62e7f5fcc1a95cbcb9a8632fab792a9835289` |
| `gdextension_interface.json` | `135740c3c51c83171a96f8f1f912837eec59c04edd0a3c604039ce1141ba9c72` |
| `gdextension_library_exit_guard.h` | `ea6596c3d4b4d61c2ee14b927f103c9e0deaab753a60a52f971d9ece84ce8831` |
| `gdextension_qualification.cpp` | `e741cbec5370eee51cd680cdfec3aa06d55c63153df91fe06c20741d53e2d7f5` |
| `gdextension_qualification_abi.cpp` | `c606a01ab8922bd741b4f2b12c258fea4ea6802029937a4698d8592b5900ed1d` |
| `gdextension_qualification_internal.h` | `7ae23725e1d9084563823a32e3aae4f976f7aff1cd4e20c55a1bad82f24cb986` |
| `gdextension_qualification_rd.cpp` | `65b705b915d4660bd6870bca4cb51ee796b7b28554d1d30b0e2e1cc93b43d91f` |
| `gdextension_qualification_rt.cpp` | `8dd913252cbbcecebb6bea7d87cfbf1e20077a7f7183f260ca194f68a4858214` |
| `gdextension_qualification_ticket.cpp` | `901202716d0932c2f8803741b3cdf3dd45f2bd6332b7bcc978819111cac0d7a5` |

## Store native-allocator delta review — main tree, 2026-11-01

No definite must-fix found in the narrow Store allocator correction at the main-tree fingerprint below. This reviewer only read source/artifacts and updated this review; no production change, build, doctest, or GPU process was run.

The initial review's E24 allocator interpretation was wrong. Logical E24 ownership and the native E05 allocation issuer are separate. The corrected `qualification_record_prepared_v2` predicate (ticket.cpp lines 876–931 at this snapshot) preserves `METHOD_DESTINATION -> METHOD_RESOURCE` role mapping, then requires every Method range's same-ticket original plan to match role, RID, backing, byte offset and exact byte size with native `owner_kind=E05`. Method ranges cannot pass with a null `qualification_return`. The physical pointer must equal a real original storage child in this bridge; the plan's E05 registry/child reference, original allocation generation, child RID/backing, live lifecycle state and bridge identity must all match. Ordinary RID/backing numbers do not grant admission. Native destination allocation generation is now checked against its actual original child, not accepted as an unauthenticated E24 projection.

This supports the requested current Store path using its original E05 destination allocation without inventing a RawResults allocation or replacing the logical destination role. The predicate does not establish the external ANGE/E24 provider grant, which remains the caller-owner contract. Existing upload admission already rejects labeling a genuine nonzero E05 backing as owner_kind E24; the record predicate is now consistent with that existing rule.

### RED validity

The retained `runtime/results/20261101-110408/STORE_DESTINATION_RED.json` and raw log provide an accurate narrow behavioral RED against pinned executable SHA256 `9e24b2281bf0b02735fb21b3e433a6142fe945d2856fd41c6f5f7d9965ab4575`. This reviewer independently recomputed and matched the executable, controlled-build metadata, RED log, probe source, pinned header and probe DSO hashes against the receipt.

The raw Metal log shows the invalid E24-labeled E05 plan refused by prepare with a zero ticket; the genuine E05 plan prepared and exactly replayed successfully; all four original upload calls succeeded; then QRT kind10 refused role 2 / upload role 18 / owner kind 1 / nonzero backing with native error 4 and reason 16. The process exited 91 exactly at `record_prepared_v2`, with no success marker. The old predicate's E24-only destination branch accounts for this refusal. This is not a syntax-only or unrelated setup RED.

The probe binds the same actual destination RID as shader binding 0 and readback range 0, with no additional RawResults allocation. It reuses a simple native control shader whose block is named RawResults; that identifier does not create another buffer. The control isolates destination-role/allocator admission and eventual exact native bytes; it does not execute the real ANGE Store method/provider. The RED exits before result completion and cleanup, and the receipt correctly makes no physical-cleanup success claim.

### Evidence boundary after the delta

The prior `runtime/results/20261101-105953/results.json` records successful native V2 positive, uploaded-prefix cancellation and V1 ticket/upload neighbors, with all recorded checks true and host unchanged. `RECEIPT.md` reports focused 7 cases/456 assertions and qualification 184/3134 plus the empty original build source-drift window. Those precede this Store allocator correction; they do not substitute for its fresh GREEN. Store GREEN and any accompanying new-build tests remain the implementation owner's pending work at this review snapshot.

The JSON schema is byte-identical to the candidate's schema. Main-tree ABI assertions differ only in formatting (`git diff --no-index --ignore-all-space` is empty); no ABI field/signature change accompanies this fix.

### Store delta fingerprints

| File relative to repository | SHA-256 |
| --- | --- |
| `core/extension/gdextension_qualification_ticket.cpp` | `2b8627c6a44f764f54c84496c38d23791365b05f81f90eeb649748dd915f0fa1` |
| `core/extension/gdextension_interface.json` | `135740c3c51c83171a96f8f1f912837eec59c04edd0a3c604039ce1141ba9c72` |
| `core/extension/gdextension_qualification_abi.cpp` | `23a5126631bb8d72b24e07bc92be4dab9865edc20a9e16e9983e5c5da698f252` |
| `work/native-multireadback-20261101/runtime/v2_store_destination_probe.cpp` | `6c7bafe75006721ebe0e5af977c0ec62ecacc097f01079e721de7fdcab088e39` |
| `work/native-multireadback-20261101/runtime/results/20261101-110408/STORE_DESTINATION_RED.json` | `438f1a4d21973c7f551787766d38155d302c2ad274c7f4bc246b53208ffe0771` |
| `work/native-multireadback-20261101/runtime/results/20261101-110408/store_destination.log` | `a30c819125774ad5c5c5ef08a8f31c60b4c85843981621b5fe93fecf5cbdd178` |

## Empty-domain V2 delta review — main tree, 2026-11-01

No definite must-fix found in the narrow empty-domain admission delta. Review covered the three changed core files, the new `empty_range_dispatch` CPU test, original V1/QRT entry paths, native empty-domain probe and retained RED artifacts. No engine, compiler, scons or test runner was executed. Only this review file was updated.

### Admission, dispatch and legacy separation

- V2 POD validation accepts zero samples only with the exact canonical range `0..0` and `body_groups_x=1`. Zero groups and a shifted empty range such as `1..1` are rejected. For positive counts up to 512, `MAX(1, ceil(count/64))` equals the prior group calculation; the upper bound short-circuits before arithmetic could overflow.
- The new private common-preflight `p_allow_empty` defaults to false. Both direct V1 callers use that default. Only `qualification_record_prepared_v2`, after V2 descriptor and ticket-mode validation, explicitly passes true for its internal legacy-shaped projection. QRT kinds 6/9 retain their `!sample_count` checks and their unchanged request layouts; V1 record/observe still reject V2-mode tickets. No public V1 empty-domain bypass was found.
- Common preflight still compares both actual push envelopes to `sample_count`, so accepted empty records carry envelope zero. It still requires valid pipelines, group limits, resource ownership and physical pins.
- Append remains an actual Capture dispatch of one group followed by an actual Body dispatch with the admitted group count, therefore one group for empty V2. There is no skipped Body, zero-group shortcut or synthesized sample/domain of one. The graph/native capacity still includes the original two graph operations plus each range's copy bound and prepared uploads.
- Readback source ownership, per-range nonzero sizes, the three Method roles, real BufferGetDataResult closures, exact aggregate copying, Body ordering, native completion and physical-return gates are unchanged. Empty logical work does not become empty physical obligations.

### Tests and native RED

The new CPU case is a bounded admission test: canonical empty V2 is accepted by POD validation; groups zero and shifted empty ranges are rejected; default V1 preflight rejects zero samples; explicit V2 allowance reaches the unsupported-native-driver gate without resources or graph commands. It deliberately does not claim to execute Body, and this reviewer did not run it.

The old pinned `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58` host RED is valid for the precise previous validation failure. The raw log has successful genuine E05 prepare/replay and four successful uploads, then an exact QRT kind10 **reserve** refusal with sample0/range0..0/groups1 and error31/reason2. No kind10 dispatched-return marker exists. Exit91 occurs at `record_prepared_v2_empty_domain`; the receipt correctly does not claim Body execution, completion or cleanup on this RED.

This reviewer independently matched host, controlled-build metadata, probe source/header/DSO and raw RED log hashes to the receipt. Empty-container GLSL source and compiled-container hashes also match their generation receipt. The independent probe uses actual runtime guards at the start of both Capture and Body GLSL: envelope zero returns before any buffer write. Its expected copied bytes are six original `0xa5a5a5a5` destination words, four original zero FaultPlane words and zero ResultEpilogue. It requires a real semantic Body graph fact with successful native observations and three distinct async result serials; the single dispatch-group count follows the admitted descriptor and unchanged native append code, not a separately exposed group-count field in the fact.

JSON schema, full ABI assertions and QRT transport source have no changes against the current committed base for this delta. New-host GREEN remains pending at this snapshot; the RED and source review are not its substitute. The probe remains a native empty-domain control, not original ANGE C06/E24 provider acceptance.

### Empty-domain delta fingerprints

| File relative to repository | SHA-256 |
| --- | --- |
| `core/extension/gdextension_library_exit_guard.h` | `ac758224538f23a394e9977238b637c9226e987cf7dfdda54b7a85fb3a472fa1` |
| `core/extension/gdextension_qualification_internal.h` | `5ee15ba127548ddefbd7294258178252394094f7880e505bf5bab0c36613139c` |
| `core/extension/gdextension_qualification_ticket.cpp` | `a8d85ea3277ccf24ea4546b3376e91724e9c9548d60864289170fb261cf7b1b8` |
| `tests/servers/rendering/test_qualification_ticket.cpp` | `4f1cc1d15754872daa58cd0e02011017e6025656fc48976bbbae6b72a9b1136f` |
| `work/native-multireadback-20261101/runtime/v2_empty_domain_probe.cpp` | `86c013786d5cbc630f695e4cba1751ef87964d4a9bf5ebc875e1b4f6a3cad7c4` |
| `work/native-multireadback-20261101/runtime/empty_containers/capture.glsl` | `f22852df28aae06207e9c677fe961f7d1924a20e792f5a23d904aaba8e4bcfab` |
| `work/native-multireadback-20261101/runtime/empty_containers/body.glsl` | `d891cc58c17e27db5716f73b190804cd73f26b299580bb6542198673f949c3f9` |
| `work/native-multireadback-20261101/runtime/results/20261101-111915/EMPTY_DOMAIN_RED.json` | `277ed7bea3c49e4535073f64a1d1c67c386272200db629823dcd29c8e560ec10` |
| `work/native-multireadback-20261101/runtime/results/20261101-111915/empty_domain.log` | `79b030835d4a4da146970b74b3fe2cbac8f283b0fa8ea73096705187a10c80fb` |
