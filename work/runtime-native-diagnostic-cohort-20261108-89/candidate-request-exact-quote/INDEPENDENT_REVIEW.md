# Independent review — per-request requirements draft

Reviewed 2026-11-08. Exact reviewed patch SHA256: `4fcbd925fc9f82acb652896e3a7ef8499ba23b607c0fa8a14fb88b8e7ca286e3`; base `75cdc4b7965b719eb81dc994239db82d10ad5f6c`. This verdict applies to that patch, not a later test/documentation revision.

## Verdict

No source underquoting, ABI wiring, arithmetic, or authority blocker found in the reviewed additive sizing implementation. It supplies a conservative per-request requirement, not an exact observation or reservation guarantee. It is suitable to continue as a draft. The included test does not yet substantiate the stated typed producer/reader scenario, and nonzero-worker/live-profile behavior remains untested. Those are acceptance gaps before adopting it for Runtime96.

Independent checks: verified the patch SHA, all seven pinned base-source hashes from Git, clean patch replay in a temporary directory, and byte-for-byte agreement of every replayed file with `after/`. No live source edits, compilation, builds, generator execution, tests, native issue, or GPU execution occurred. Author generator/apply evidence is not runtime evidence.

## Staging graph bound

The `ceil(bytes/32)` bound is conservative for the original captured upload and readback paths, given nonzero uint32 length and block_size >=32:

- Both paths call `_staging_buffer_allocate` with required alignment32 and amount=min(remaining,block_size).
- A successful partial chunk in a currently used block is rounded down to a multiple of32 and permitted only when available>=32. A fresh-block chunk is the requested amount: if it is nonfinal, block_size>=32; a final chunk may be smaller.
- Thus k successful chunks consume at least32*(k-1)+1 bytes, yielding k<=ceil(bytes/32). Alignment holes consume staging space but create no extra source segment. Refusal before a successful chunk does not add a segment or advance consumption; retries resume the suffix.
- Readback publishes one actual captured graph handle per copied region. Upload aggregates one or more copied regions into a graph handle, so counting chunks remains an upper bound. Mutability synchronization commands are not additional captured source graph memberships; command arena sizing remains the original `record_source_slot_bound` policy.
- Zero and >UINT32_MAX lengths are rejected before arithmetic; quotient plus remainder avoids addition overflow. UINT32_MAX gives134217728, fitting uint32. The bound can be too conservative for the existing pool, in which case range refusal is correct. It must not be reduced to ceil(bytes/block_size) under shared partially filled blocks.

This proof covers the original ordinary captured buffer paths only. It does not establish raster/native-output closure, arbitrary future source paths, or protection against later staging configuration changes; the draft rejects unsupported families and retains original append checks.

## Native, arena, and return accounting

- The query calls the original `production_membership_bound_locked` while holding Registry and a nonblocking RD lock. It uses the original driver profile, including current pending completion records, and explicitly includes the existing post-graph PREPARE source. It does not substitute a hardcoded native-per-graph constant.
- Distinct declared STORAGE slots per action reproduce the existing reserve worker-bound loop. The maximum across actions, clamped to the original RD worker maximum, is conservative when the original preparation/closure validation succeeds. The changed reserve call passes this same bound to its later device quote, removing the inconsistent resource-count worker check. Default callers of device quote preserve the old resource-count behavior.
- Graph command slots/frames use the original layout and current original graph prefix, with the original maximum-worker command arena policy. The accepted pool's per-request prefix multiplication remains unchanged. Profile or prefix drift can still make reserve/append fail; the quote grants no frozen native state or contiguous free slice.
- `return_cells=resource_count` matches `returns[resource_slot]`, including retained resources. Counting only owned resources would underallocate this representation. `resource_uses=max(resource_count,use_count+binding_count)` matches original capacity constraints and appended descriptor rows; the sum is checked before addition.
- Payload is an explicit frozen caller total, with upload bytes independently summed and bounded by it. Original push/upload preparation consumes unaligned byte totals, so no hidden per-payload padding is omitted. Readback bytes are summed with overflow checks and matched one-to-one to READBACK action slots.
- Final `capacity_fits` compares every derived capacity with the accepted pool. It does not test current free ranges, which remain reserve's responsibility.

## ABI and failure behavior

The JSON entry, Registry declaration/definition, thin wrapper, exact function-pointer typedef assertion, and interface registration are all present. The draft header is generated rather than manually substituted as an installed ABI. The V2 action rows and explicit source readback rows have no abi header by design; typed entry selection chooses their layout. UploadV2 retains and validates its V2 abi header/reserved fields. The additive V1 function name is therefore not a V1 reinterpretation of action rows.

Input/output alignment, count extents, pointer-end arithmetic and disjoint input/output spans are checked before row reads. Scan counts are bounded by the accepted pool before nested loops. Matching upload rows are unique; orphan uploads and unmatched/multiply claimed readback rows fail. Public output is written only at the final success, so all earlier error paths preserve it. The method allocates no serial, request, source lease, native object, arena reservation, operation, or producer/consumer grant. Attachment authentication and owner-thread/lifetime checks use the original Registry path; RD acquisition is try-lock only.

## Validation contract boundary

The query deliberately does not repeat all reserve row validation. For example, it can currently quote rows with unknown resource ownership, invalid use access/list_boundary, or an unknown readback source_kind/inactive source payload, provided their sizing fields are otherwise usable. Original reserve rejects these later; these particular tags do not change the count calculation, so I found no underquote or authority escalation from that acceptance.

Before freezing this public draft, either explicitly document that success validates sizing fields only and may size structurally invalid rows, or reuse pure structural checks for enum tags, inactive payloads, generations and action correlations. Do not imply that query success validates the original frozen closure or source semantics. Full logical/source authority must remain outside this sizing query. The author has been informed of this contract clarification.

## Concrete acceptance gaps

1. **Original two-request test is not a typed reader test.** `after/tests/core/extension/test_production_request.cpp:1044` calls `Enrollment::reserve` twice; that helper reserves a V1 upload metadata request both times. It does keep the first original request alive, preserve the same accepted pool, and verify separate graph/native/return pointers plus disjoint command ranges. But it never calls the new registered interface through its generated typedef, V2 reserve, or an explicit incoming readback source. Fix the test to use the typed query and actual V2 upload producer/readback reader metadata requests; verify disjoint ranges including frames/use/resource/payload/readback arenas as applicable. Label it allocator/typed metadata evidence only: even that revision cannot prove the authenticated producer-carrier join or GPU order. The author is already revising this test.
2. **Exercise live native accounting rather than QuoteDriver's zero-worker one-credit case alone.** Add a fixture with a nonzero worker bound, pending-completion/profile contribution, explicit post-graph prepare cost, and current prefix. Verify the returned native count follows the original profile; mutate profile/prefix after quoting and require later reserve/append to refuse safely without corrupting the retained first request. The original test's zero-worker QuoteDriver does not cover the changed worker-bound call.
3. **Complete supported-branch and failure coverage before runtime adoption.** Current tests cover one upload and scalar helper boundaries, not an actual readback sizing call, dispatch/copy graphs, malformed spans, arithmetic totals, or unmatched readback slots. Cover all four ordinary action branches and output-unchanged failures. Run them after the parent authorizes compilation; none have been run by this reviewer.

The two accepted original requests and their retained arenas remain necessary infrastructure for SAME89/90; this query alone cannot authenticate stable producer bytes, checked same-graph order, source retention, or logical completion. No pool enlargement or arbitrary partitioning is introduced by this patch.
