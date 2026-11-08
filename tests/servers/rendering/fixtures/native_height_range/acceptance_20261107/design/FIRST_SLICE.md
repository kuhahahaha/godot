# First directly implementable slice: original RD-owned D32 clear range

This is the next concrete native implementation batch. It does not wait for a remote/extension issuer. D's original RenderingDevice creates, records, seals, joins, retains, polls and returns this actual range. Source writes remain pending root authorization after current quote validation.

## Private API and exact first behavior

```
Error _native_height_range_prepare(Ref<NativeHeightRange> &out);
Error _native_height_range_record_depth_clear(const Ref<NativeHeightRange> &range,
    const NativeDisposalWitness &depth, const NativeDisposalWitness &framebuffer,
    float clear_depth);
Error _native_height_range_close(const Ref<NativeHeightRange> &range);
Error _native_height_range_observe(const Ref<NativeHeightRange> &range,
    const NativeDisposalBudget &budget, NativeDisposalUsage &usage,
    NativeHeightRange::Snapshot &out);
Error _native_height_range_return(const Ref<NativeHeightRange> &range,
    const NativeDisposalBudget &budget, NativeDisposalUsage &usage);
```

No caller-provided RecordHandle, frame index/epoch, native serial, accepted bit, completion bit or OriginalOperationIdentity is accepted. Ref construction/mutation belongs to RD. Existing native friend/facet may retain the object; no public ABI or token registry is added. The dedicated header's destructor is public/no-op native-wise; constructor and mutation remain private. Snapshot gives original mechanical source facts/last-use, not E09 or Scene content authority.

First slice supports **one exclusive standalone D32 clear pass in one graph epoch per cell**, with multiple independent closed cells per Frame and multiple Frames in flight. No scene Frame/content/budget/caller is needed to exercise it: an original native test fixture calls the private API with the actual native depth/FB witnesses. Existing real D32 resource fixture can demonstrate actual append→seal→issue→poll→return on Metal.

Prepare reserves finite memory before recording. Start with an explicit64 live-cell pool limit on original RD; excess prepare returns BUSY without native effect. Per-cell graph-member capacity is derived from the actual fixed checked-result arrays: DrawListBeginResult begin+clear+preparation[1]+prefix[2], plus DrawAppendResult end draw_handle+prefix[PREFIX_CAPACITY]. Checked arithmetic yields38 maximum slots at today's arrays, with dedup; changing those definitions changes the compile-time derivation rather than a silent hand-estimate. Source capacity comes from original `driver->native_issue_bound(MAIN_SLOT,1,bound)` and observation-supported capability. Edge capacity is checked graph-capacity*source-capacity. Allocation failure/unsupported/bound overflow is zero new recording/native effect.

Before the specialized append, validate original RD/render thread, empty/unrecorded selected cell, current actual Frame generation, closed recording error state, no open draw/compute list, no RDG driver callbacks, and real LIVE standalone D32 depth+FB witness/cache/attachment identity using the existing native pair validation semantics. Reject view/fallback/transfer-worker resources and pending_clear before effect. These restrictions define the first supported recipe; they do not masquerade as worker/hidden support. A pending unrelated hidden copy source is not enrolled into this range just for sharing the Frame. This recipe does not encode hidden copy work or resource uploads. Actual command batch may include unrelated graph commands; joining its original shared native call is legitimate, while graph membership remains exact to this clear's actual records.

The append method itself calls the original `_draw_list_begin_recorded` with DRAW_CLEAR_DEPTH and `_draw_list_end_recorded`. Store every actual begin/clear/preparation/prefix/closed-list handle from those original results; use no begin/end count inference. It owns no arbitrary renderer list. If begin fails before effect, record its error with no invented append. If a partial prefix exists, retain all real handles and mark failure. Only unwind a list proven opened/owned by this exact original begin result; use original checked end once, never reset global state or end a caller's unrelated list. Failed/unknown restoration keeps original Frame/range ownership for original recovery, no publishable range success.

Register the range Ref into the original Frame+epoch list BEFORE calling begin, after all finite preflight, so even a failed actual prefix has custody. `close` freezes enrollment; it does not submit or infer acceptance. Empty/rejected capture reports no_recorded_effect, never a successful height write/native_complete. Actual graph mapping and issue can occur later on the normal or bounded original path.

## One cell, exact added fields

In the native range: immutable device instance and RD mechanical serial; finite graph members `{actual handle,frame,epoch,sealed,slot,command_buffer,issue_attempted}`; unique actual sources `{immutable NativeCallFact,retained,complete,observer_returned,retention_error,completion_error,completion_native_code}`; graph→source edges; flags recording_closed/has_recorded_effect/mapping_failed/uncaptured_prefix; separate sticky append/mapping/issue errors; retain/poll/return cursors. No raw RD/driver pointer is retained.

In RD: finite retained cell pool, next internal range serial, private methods above and seal/issue/retain helpers. In original Frame: bounded retained `native_height_ranges` refs. Keeping the same actual cell reachable from Frame and RD protects early client Ref drop and recycled Frame indices. Sources are unique by actual device+serial+queue+completion serial, with immutable-fact consistency checks. Distinct ranges can retain the same original native source independently; their observer obligations must not be merged into an unbounded new owner table.

## Actual hook insertions (both engine paths)

1. Original `_end_frame`: after draw_graph.end and before graph reset, beside `_seal_buffer_readback_records`, call `_native_height_seal_frame(frame)`. Iterate only this Frame's current-epoch cells. Resolve each actual handle with original `record_mapping_state`, `resolve_record_command_buffer_slot` and `resolve_record_command_buffer`; store slot AND exact command buffer. A failed/unknown mapping sticks to this cell; mapping is no issue fact. Insert the same helper in the bounded continuation immediately after its actual graph end/seal phase.
2. Original execute_chained_cmds: beside `_observe_buffer_readback_issue(i,command,queue_error,report)`, call `_native_height_issue_slot` on matching sealed cell members. Add the identical helper to the bounded actual issue loop (which does not call execute_chained_cmds). Empty BUSY report does not mark issue_attempted or attach source; resume original unissued slot. A nonempty rejected/unknown report marks the actual attempted slot and preserves raw error/prefix. Never issue a second time to repair metadata.
3. `_native_height_issue_slot` pre-reserved source/edge storage from prepare. Attach actual report calls to the exact member/slot+buffer. Ignore presentation-only diagnostics as GPU obligations; NotIssued is no accepted native source. Missing/disabled completion identity after an actual issue is failure/uncaptured-prefix, not no-effect. Use report facts verbatim and keep original raw issue error separate from later completion errors. Immediately attempt retain, or leave exact pending row whose original owner is protected by the next barrier. If backend exceeds its declared bound, keep the original Frame ownership and fail closed; never drop an actual prefix or advance recycle.
4. `_stall_for_frame`: before any native_call_recycle, hidden observer release or final fence recycle, invoke `_native_height_retain_frame_sources(p_frame)` independent of `_production_retain_frame_sources`. `_native_disposal_stall_step`: invoke the same barrier at entry before any return phase. It uses original existing retain work admission and driver.native_call_retain on the same serial; BUSY blocks original source retirement and resumes later without duplicate retain/issue. Audit original callers so a temporary BUSY retains the pending original Frame rather than forgetting/reissuing it.
5. Once retained, Frame may recycle/reset normally. The cell's source identity survives via driver observer counts. New original RD observe step calls only native_call_poll(original.serial), checks exact completion_serial and KnownAccepted or same-source original resolved_issue refinement, and preserves raw issue/native errors. No waits, tail-fence inference, elapsed-time promotion or NativeSourceOrigin manufacture. Closed enrollment + all valid sealed/attempted members + all required accepted sources authoritative Complete is the only native_complete=true path.
6. Explicit `_native_height_range_return` releases each successfully retained original serial at most once, through existing RELEASE budget admission. Only a completed range may normally return successful observers; unknown/error sources stay unresolved until the existing original device-failure/retirement path actually permits release. Release BUSY retains the exact cursor/cell. Keep final native-complete observation immutable after return so read-only snapshot does not poll a freed source. Remove RD/Frame cell refs only once source ownership is settled; Scene Ref drop alone never drops an observer.
7. Original RD finalization calls a private range-drain guard before driver destruction. It must preserve unresolved actual sources/owner on BUSY/error, not destruct raw source references after the driver dies. No resource free/refund path is added.

## Narrow write set

- New `servers/rendering/native_height_range.h` (finite typed cell/report).
- `servers/rendering/rendering_device.h/.cpp` (private creation/append/Frame membership/seal/actual-issue/retain/poll/return/finalization hooks).
- `tests/servers/rendering/test_native_disposal.cpp` or a dedicated native-range test file using existing private test-access style (author writes tests first; root executes).
- Existing real native-depth-allocation test fixture/runner only as independently authorized by root to call the new range API. Existing native driver CompletionRecord issuer is reused unchanged in this first slice; no driver or extension ABI implementation is required for this recipe.

## Required mechanical acceptance for first slice

| Case | Expected original effect / negative control |
|---|---|
| Real D32 clear, no geometry | Actual begin/clear/end graph handles, exact slot+buffer, real original native source; native_complete after its callback only |
| Ordinary frame and bounded no-nextdraw continuation | Both actual seal/issue helpers advance same cell; deleting either hook fails its branch |
| Two cells same Frame/shared native batch | Exact separate graph memberships, same actual native source may be retained twice; returning one does not lose the other's observation |
| Multi-slot Frame (including unrelated earlier/later commands) | Only actual clear's sealed slot attaches; final fence never substitutes; wrong buffer check fails |
| Empty BUSY queue retry | No new issue/source row; single eventual actual attempt; no duplicated native observer |
| Actual early accepted source then later frame failure | Real prefix stays retained, unrelated unissued member cannot be Complete |
| Retain BUSY before ordinary and bounded recycle | Frame cannot recycle original source; retry advances once; deleting retain barrier fails |
| Actual callback pending/wrong serial/unknown issue/native error | native_complete false; raw issue and completion errors preserved separately |
| Metal IssuedUnknown callback refinement | Same original callback may establish KnownAccepted+Complete; quiet tail fence cannot |
| Client drops Ref before issue | Original RD/Frame cell still holds prefix and can finish/return without UAF |
| Frame reuse, old Ref observed | Old exact source remains; new graph epoch/driver reuse does not retarget it |
| Explicit return BUSY/retry | One release per retained source, stable cursor; no secret destructor driver work |
| Bound/cell capacity exhaustion, invalid witnesses/open list/callback/transfer | Refuse before new recording/native effect; original BOUND gates unchanged |
| Failed/unknown checked begin/end | Preserve real prefix and original list ownership; do not mark empty/rejected capture complete |

CPU fake-driver mechanics are not real GPU proof. Root's actual Metal fixture must verify native_complete before requesting resource retirement, then original physical witness RETURNED separately. Complete the first slice only after both normal and bounded paths plus meaningful retain/slot negative controls pass. Subsequent slices below add actual height draws, hidden/worker dependencies and readers; do not claim them from this clear-only acceptance.

## Pre-effect worker/hidden exclusion and exact mapping proof

Source evidence below is the immutable v2 RD input, not root's current controlled mutation. `_draw_list_begin_recorded` at6763 preflights capture D32+one attachment+no owner/shared fallback/worker marker, then calls `_texture_check_pending_clear`, `_texture_update_shared_fallback`, RDG begin, usages and original viewport/scissor. `_texture_update_shared_fallback` at3064 immediately returns for the required null shared_fallback; its copying branch is excluded before entering begin. `_texture_check_pending_clear` at3274 does nothing if pending_clear=false; otherwise it consumes `_check_transfer_worker_texture` (preflight must have transfer_worker_index<0) and records original RDG depth-clear command. It does not issue a hidden Metal copy queue in this standalone D32 recipe. The original checked begin returns that at-most-one real preparation handle, followed by real clear/header and viewport/scissor handles. `_draw_list_end_recorded` at8093 adds the actual closed list and unbinds original attachments; it does not submit an upload or touch a transfer worker.

Accordingly first-slice preflight must explicitly require: depth owner invalid; shared_fallback null; transfer_worker_index<0; native_external_access false; no production/qualification physical attachment; no requested retirement; exact LIVE depth witness and exact LIVE one-depth-attachment FB/cache witness; no existing draw/compute/raytracing active list; valid current graph/Frame epoch; no RDG driver callbacks. Querying/validating those fields is before the first checked begin call. It must reject unsupported branches at this point, not discover a transfer after recording. For a decisive first-batch preflight, reject pending_clear=true before checked begin even though its normal depth-clear path records a bounded original preparation. This makes the first supported recipe unambiguous and tests the zero-effect refusal directly. The real fixture must report the actual pending_clear flag and use the supported original standalone D32 state; if that driver requires initial clear, perform that original preparation outside this first clear-range recipe and retain its existing source ownership, then record the new actual clear range. A later preparation/hidden/worker slice removes only the restriction it actually implements.

Original common `NativeSourceAdmissionScope` stays in ordinary end/execute. Unrelated earlier hidden copy queue flushes retain their existing Frame owner; they are not related to this direct clear recipe merely because the Frame contains them. First slice neither creates hidden copies nor modifies existing hidden observer ownership. In particular it does not set qualification_frame, mint a qualification ticket, enroll a ProductionRequest or call a budget ledger to obtain completion.

Original RDG `_resolve_record_command_index`/contains_record maps closed draw-list begin, clear, instructions and end to the real closed TYPE_DRAW_LIST command by original list index. `record_mapping_state` requires original graph instance/frame and a successful graph-end command-buffer mapping; `resolve_record_command_buffer_slot` and buffer resolver require original begun slot and exact nonzero buffer. First-slice seal must check BOTH resolvers for EVERY saved actual handle and fail if any member is Pending/Failed/Unknown/Invalid after graph end. Dedup of several handles mapping to one command must not skip validating each handle. This closes the complete begin/preparation/clear/prefix/end mapping before issue.

Recycling hooks are independent additions in RD only: new `_native_height_retain_frame_sources` uses existing native-disposal service work admission and driver observer calls on this cell. It never accesses ProductionRequest, U08, E05 claim or Registry; existing production/ticket retention remains separately in the original call order. That separation must be enforced by source review and a fixture with no extension/production attachment.
