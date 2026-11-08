# Current applicability review — prospective G prerequisite on 74a

2026-11-08, independent read-only Sol review. Reviewed `prospective-g-closure-head80a.patch` against current HEAD `74a28657f0956d46056adb34f8b10f17a0716243`, with historical `INDEPENDENT_REVIEW.md`, `DELIVERY_HEAD80A.md`, `REPLAY_HEAD80A.json` and the actual relevant source. No live source edits, compile/build, runtime/mutation run or cross-session message occurred. Only this report was written.

## Verdict

**No concrete blocking source defect or current-HEAD applicability conflict found for this prerequisite slice.** It can proceed with the scope stated in DELIVERY_HEAD80A. It is not full-G admission: missing authenticated orchestration, per-group reservation validation, raw-root/physical eligibility checks and aggregate increment proofs remain integration requirements, not a reason to reject this explicitly narrower extraction/helper.

The reviewed patch is SHA-256 `cc7120d3b5e97719286860a46cb26b9e72c56ae00c28393cad16f66e2423d708`. Apply **one** alternative: the full prerequisite patch reviewed here, or the smaller shared-enumerator alternative; do not apply both. This review's semantic findings concern the full prerequisite patch.

## Delta and test preservation

Fresh `git apply --check` against the current workspace passed. `git diff 80a75b… 74a286… --` for all three target paths is empty: the r2 requirements/SDK work did not modify these targets.

Independently replayed every unified-diff hunk **in memory**, requiring each context/deletion line to equal current source and comparing the result byte-for-byte with `replay-head80a/`. All three match. No temporary checkout or live apply was used.

| Target | Current 74a source SHA-256 | In-memory patched SHA-256 |
|---|---|---|
| servers/rendering/rendering_device.h | `31efa34b295ac21dfcea509b4879fd3a8fa4ac421356552f2110d0fe9f57ec0e` | `8f8a56fc76ae2d6eada61ba2984800c4e64364509f61232fb733c5ff6ffc02a4` |
| servers/rendering/rendering_device_immutable_hold.cpp | `19dcec89608d5e85e391d310a764496983ad2a6560043e4015b2aa4e01551489` | `cc35436afa0985707a07e2c0225164bf69a04d78cce1f39b96856a038afea198` |
| tests/servers/rendering/test_mesh_storage_external.cpp | `ea82ce7870b9451ed86a1b1b88f7dd849593f596b54c9bcbd46959da7d7ab6a8` | `3c8e3c9f44e17c3a3a3fdac9143ce60c4b4eb99b7089d2922c1b325dce3e73b5` |

Both patched production files are also byte-identical to the historical reviewed `after/` versions. The new review therefore focuses on integration-base/test retention rather than suggesting production logic changed since that review. The historical full patch SHA `3764b914…` and current rebased patch SHA `cc7120d3…` are distinct artifacts.

The current test file's **10 existing TEST_CASE registrations are all retained**, with exactly one new ProspectiveClosureKeys case. The rebased test delta has additions only; it preserves the later original mesh revision, failed-mutation, shadow notification, saturation and guarded-pointer cases. The first insertion closes the pre-existing routine with its same dispose/free-count/end-native sequence before adding the new helper; it does not remove that routine's cleanup. Historical syntax receipts are author evidence only; they are not a fresh compile of 74a+this patch. The new focused test adds no REQUIRE-based setup continuation or unchecked pointer dereference.

## Source checks repeated at the actual seams

**Acquire reverse versus stage bidirectional.** `_add_dependency(id, backing)` at RD cpp178 inserts id into dependency_map[backing] and backing into reverse_dependency_map[id]. The new enumerator at patched immutable cpp365 visits forward-map neighbors only when bidirectional, then reverse-map neighbors in both modes. Actual acquire calls it with false at438; actual stage calls it with true at1143. Ordering matches both removed loops. Shader→embedded samplers and texture view→owner remain directed in both modes; no reverse global scan is added. Stage retains its existing fixed-point loop and rollback on enumeration/predicate failure; acquire still fails before publishing a hold. There is no graph-direction swap.

**Original owner predicate.** Patched cpp955 extracts disposal head/incarnation/driver/counter checks and original qualified-use and material callback ownership from actual staging. It preserves request pointer/serial/RID + same_physical/linked relationships, imported original backing identity, the original prior-use scan and transfer-worker error distinctions. `kind<10` driver handling and the absence of an additional render-pipeline qualification branch match the old stage. This is the stage predicate, not the stronger acquisition eligibility predicate.

Stage first validates each of at most two actual holds (device, pin state, nonempty span, no duplicate hold); only then copies each hold's material/generation into a stack array. The extracted callback check receives just this original batch, still requires the exact set provenance and `_native_hold_callback_is_owned`, and neither runs a callback nor acquires a new lock. The copied origins cannot be interpreted as caller-issued authority. Future orchestration must preserve origins separately for each actual group: a material origin valid for its material group cannot authorize a geometry group through a shared callback-owned uniform set.

**Typed future keys.** The private scratch helper at849 distinguishes `{actual RID, future=UINT32_MAX}` from `{null RID, valid future ordinal}` and rejects malformed combinations. It deduplicates complete typed keys, never manufactures a future RID. Reachable future VA symbols expand only original vertex-buffer owner dependencies; reachable future pipeline symbols require exactly one shader-owner dependency. Actual reached nodes must resolve through `_native_immutable_counter` or fail. Bidirectional mode scans virtual incoming edges from each reached actual node, so a future VA discovered later via an old VA's shared backing expands its other backings. Reverse mode does not follow virtual incoming edges. This is a correct keys/counts traversal within its declared contract; exact ready shader, used-buffer plan, future factory identity/coalescing and lifetimes must still come from retained original plans.

**Overflow/publication.** `nodes.size()` uses CowData::Size=int64; limit is uint32. The uint64 size comparison occurs before `resize(index+1)`, so the allowed UINT32_MAX bound cannot wrap int64 growth. Resize failure is handled. The current key is copied before append can reallocate the vector. Output is cleared before computation and published only after complete success. The extracted predicate computes use_count+binding_used in uint64 and rejects exceeding capacity for its own scan. Existing stage/adopt/inventory code still has older uint32 begin arithmetic; the patch does not claim to repair it. Original duplicate staged-resource pins retain their existing `pins > UINT32_MAX - remaining` guard. None of these local checks proves aggregate future resource/device increments before any new physical hold is acquired.

## Evidence and remaining gates

The added focused test exercises actual a→old VA→b→future VA→c closure (5 nodes), reverse-only direction (1 actual-root node, or 3 future-root nodes), exact limit4 refusal/empty output and selected no-effect counters/maps/factory counts. Source-level expected topology is consistent. It does not execute new physical acquisition/staging via the enumerator, future pipeline keys, embedded directed links, qualified-use/callback refusals, reservations or aggregate saturation. No actual mutation/test run was performed in this review. Existing failure-control patches are proposals; in particular stage-reverse-only mutation sensitivity is explicitly unestablished, not evidence of equivalence.

Before calling this full-G rejection/admission, original integration still needs:

- authenticated preparation scope/pin/request/row identities, retained complete roots/material/geometry plans and exact ready shader/backend/future-factory keys;
- separate physical acquisition eligibility and per-hold limits, material **raw** root cap before dedup, original per-group bidirectional staging closure and exact reservation layout;
- checked combined increments for each actual resource and device hold count across both geometry holds plus material hold (MAX-1 plus two new increments must refuse before the first factory), without recounting prepaid holds;
- both original groups plus the owned pipeline group approved before effects, followed by original execution revalidation, actual final inventories/adoption and cleanup;
- production-path regression/failure controls on the rebuilt intended target. Unit/headless fixture results cannot establish GPU or full backend acceptance.

The helper presently has only its definition and focused test as callers. That intentionally open orchestration boundary is accurately documented; it does not create a hidden G grant. The current r2 ABI/requirements changes do not overlap this private RD prerequisite or alter the above source verdict.
