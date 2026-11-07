# K01 original checked-sink prefix join: source audit and narrow write set

Default Sol, read-only. This continues the existing K01 responsibility; it is not a replacement FRPLAN. No production edits, build, GPU run, source import, issuer or completion receipt was made. Current D HEAD is ad645229db609ec5b6caa0f2c793dc8b078a95d1 (including ea244 clear range), with public5 R2 frozen pending execution. Fixed Scene objects read: 8c6f2fb72af1e9152beeaa3d45dba5afcaa88d1b; f62766d37c561d59c0b7949cd73fa2003015db77. Full 2eb58a826d17b021eebc682b41ec55a9eba2153c became locally readable during this audit; its parent is 8c6f and its four-file diff is fixture cpp/h, TESTS friend in RD.h and run_resources.py (65 insertions/6 deletions), not a general enrollment implementation. Original object pins/current source hashes accompany this document.

## What the actual original sink carries

At 8c6f, `height_capture_checked_sink.h:60-74` has NativeRecord(operation, resource, index, BeginResult, BindResult, DrawResult, transfer_offset/count). It owns finite business and transfer arrays and an independent cleanup NativeRecord End slot, with attempted/ownership/restoration/first_error fields. These are actual original checked results, not proof of source membership or completion.

| Original result | Facts that must be preserved, including on refusal |
| --- | --- |
| Begin | list identity; begin_handle; clear_handle; preparation_handles[1]/count; prefix_handles[2]/count; effect/error/capture_complete. Preparation or clear can exist even when final Begin fails. |
| UniformSet bind | CPU state_accepted/state_changed/capture_complete/error/effect; usually deferred state and no graph instruction. Do not invent a graph member for an accepted no-op. |
| VA/IA bind | actual bind_handle plus state acceptance/error/effect/capture_complete; the exact `transfers[transfer_offset..count]` captured from original buffer worker/operation identities before dependency processing. Capture occurs before a possible dependency refusal. |
| Pipeline/Push/Draw/End | draw_handle, all prefix_handles/count, optional draw.transfer when has_transfer, effect/error/capture_complete. NoNewEffect is not a synthetic native operation; NativePrefixOnly or failure can still carry real prefix handles. |
| Cleanup End | separate actual `_draw_list_end_recorded` result from unwind, including every prefix/transfer/handle and its own error/effect/capture flag. It does not overwrite or erase the failed business record. |

The sink records the refused operation before returning its sticky first error. `finish()` can run explicit unwind; End with Unknown/NativePrefixOnly is not retried, and failed restoration leaves quarantine responsibility. Destructor never secretly ends a list. `is_restored` checks CPU active-list/bound-texture state and an original graph end handle, not GPU completion.

8c6f HeightFrame retains both the original HeightFrameResources Ref and complete sink. Its immutable snapshot freezes instance/base/source_generation/shape_revision/producer_occurrence/transform/extents/masks/resolution/dimensions and original allocation observations. `capture()` sets CaptureRecorded only after successful checked Begin/End, restored/no-open/no-unknown/no-cleanup and complete business capture. It explicitly does not establish RecordedOrdered, publication or native complete. Failure retains prefix in FailedRetiring.

## Existing D hooks that can be reused

- RD.h:1757 onward and RD.cpp:10188-10638: private range prepare/validate/close/observe/return, original main-RD/RT check, RD incarnation, finite graph/source/edge storage, driver NativeCallFact retention/poll/release cursors, Frame custody and finalization refusal.
- `_native_height_seal_frame` at RD.cpp:10354 resolves each exact original graph handle to actual command-buffer slot and buffer. Ordinary end-frame and bounded disposal both call it before graph recycle. `_native_height_issue_slot` at :10382 joins only matching frame/epoch/slot/command to the actual original QueueCallReport. Existing retain barriers prevent original Frame source recycle before independent range retention.
- `NativeSourceAdmissionScope` and existing RD `QualificationSourceAdmissionScope` already wrap original Begin, worker issue and graph issue paths. `core/extension/gdextension_qualification_rd.cpp:367-419` receives actual native serial admission/cancel and hidden reports. Extend the same original scope dispatch for active range custody; do not install a competing driver issuer/scope that masks existing ticket/production consumers.
- Original `_check_transfer_worker_operation` at RD.cpp:9640 holds thread then operation locks, authenticates the worker and used operation, and freezes qualification source credit when required. Original `_submit_transfer_worker` at :9461 has the actual issued report and covered operation interval before worker reuse; `_production_join_transfer_worker` at :9525 demonstrates the original exact-operation join and retain pattern. `_wait_for_transfer_worker` at :9560 and `_submit_transfer_workers` around :9786 are actual recycle/return barriers. Reuse their location/lock ordering, not their public Registry identities or ticket authority.

## Exact missing states; why clear enrollment cannot accept this prefix as-is

1. Current D lacks the Scene sink/HeightFrameResources/HeightFrame definitions and nested declarations present in fixed f627/8c6f. It retains source key/revision fields, not these owners. 8c6f adds quote-aware owner/sink seams; 2eb58 adds an opt-in fixture only. Scene must deliver its latest original classes/callers rather than D copying historical classes over active Scene work.
2. Current range prepare derives a bound for one Begin+End clear and MAIN_SLOT source only. `_record_depth_clear` is the sole enrollment caller; it deliberately excludes transfer workers, hidden copies and resource-sharing paths. No actual private checked-sink enrollment method exists.
3. GraphMember contains handle/frame/epoch/slot/command; there is no transfer dependency member, exact worker operation-to-batch join state, hidden source association, capture-sink identity or capacity reservation for a geometry prefix. Seal currently requires all members map to the same first slot/buffer. General prefixes must resolve each actual member independently; retain the clear recipe's exclusive-pass assertion only for that recipe.
4. Current `_observe` at :10536 requires append_error==OK etc. to set native_complete. `_return` at :10570 accepts only native_complete or exact known-no-effect. Thus a fully captured business failure followed by a successful cleanup End cannot mechanically return even after all of its original sources settle. General support must separate business success from physically settled known prefix. Sticky append/creation errors must remain; cleanup may return known complete sources without making that failed capture successful/Current/Complete. Missing/unknown prefix, unresolved issued state or failed retention still quarantines.
5. Frame and resource Ref retention are custody, not producer/reader ordering. No original producer-close/reader-lastuse dependency is attached to this range in current or fixed Scene code. Neither plain refcount nor a raw checked handle proves those dependencies.

## Proposed private signatures and concrete caller sequence

These are implementation recommendations, not installed APIs. Keep them private on original RD, accessed only through original C++ Scene friend/caller; no C ABI leaf, public raw-handle importer or externally constructible enrollment token.

```cpp
// Forward-declare original RendererRD::HeightCaptureCheckedSink.
Error _native_height_range_prepare_checked_capture(
    RendererRD::HeightCaptureCheckedSink &p_original_sink,
    Ref<NativeHeightRange> &r_original_range);
Error _native_height_range_enroll_checked_record(
    const Ref<NativeHeightRange> &p_original_range,
    const RendererRD::HeightCaptureCheckedSink &p_original_sink,
    uint32_t p_original_record_index, bool p_cleanup);
Error _native_height_range_close_checked_capture(
    const Ref<NativeHeightRange> &p_original_range,
    const RendererRD::HeightCaptureCheckedSink &p_original_sink);
// RD-internal only, called under original worker locks; never accepts a
// caller's QueueCallReport as authority.
Error _native_height_join_transfer_sources(TransferWorker *, bool p_operations_locked);
```

`prepare_checked_capture` runs before the first checked append: require empty original sink, `&sink.rd==this`, actual main RD/RT/preflight/open graph/current nonzero Frame epoch, no other enrollment on that sink; record the exact sink address, RD object incarnation, frame index/qualification_epoch and actual graph identity/frame. Install original Frame/range custody before any possible effect. Derive fixed graph capacity from that sink's finite business capacity plus independent cleanup slot and actual maximum result arrays; derive worker dependency/source/edge bounds with overflow checks and actual driver issue bounds/worker prefix quote. Reserve all arrays before recording. Refuse before append if unsupported/insufficient, with no overwrite of an existing owner/range.

The original sink invokes private enrollment immediately after assigning each actual result, even on error, before `accept()`/return; the enrollment routine reads original private sink storage, counts and tagged operation, not supplied copies. Each call is scoped to that sink/range while original RD checked methods execute. Bind's `_check_transfer_worker_operation` must enroll/freeze its real worker dependency under the existing locks before returning them, so an async worker cannot recycle between the actual dependency check and the sink's post-call enrollment. A late final scan alone is insufficient.

Validate index/cleanup exactly once, same sink/RD/frame/epoch, expected original Begin/list identity, and `draw_graph.contains_record` for each returned valid handle. Record full handles immediately and de-duplicate their complete identities; do not use a counter interval, tail only, resource RID or `is_valid()` alone. Graph identity checks are necessary but not authorization: live original sink custody plus original RD call scope supplies provenance. Read every Begin/Bind/Draw member and transfer slice including independent cleanup, irrespective of business error. NoNewEffect and deferred binds add no invented handle. Capacity/provenance failure after an effect marks uncaptured/unknown and blocks return.

At original worker dependency enrollment, record worker identity plus exact operation and original frame/epoch; under original locks distinguish processed (no outstanding worker dependency), current recorded/pending batch and current submitted batch. For pending/current batches install custody and bounded source credit before issue/recycle. For an already submitted unresolved operation join and retain its exact still-owned report before locks release. An operation already processed is settled only through original worker state under those locks, not a caller assertion. Do not borrow a later reused `last_submission` or join every Frame report.

At actual worker submit, join only registered operations covered by that exact first..last operation interval to its actual report/native facts. Attach hidden reports only from the original active sink call/native admission scope, before callbacks can be lost. Preserve original serial/completion serial/queue/error/native code/issue capability; share/deduplicate actual source facts with edges without granting another consumer's identity. Range-specific retain must independently succeed before original worker fence/command-pool/staging recycle. ERR_BUSY resumes retention/observation, never reissues the native batch. Keep existing qualification/production gates in place.

`close_checked_capture` requires final original finish/unwind state, captures the independent cleanup slot, verifies record/transfer counts accounted, and closes recording only; it issues no Complete. Range success remains conditional on a clean business capture plus all original exact source facts. Add an internal known-prefix-settled predicate for physical observer return independent of sticky business error. A captured failure with fully known/accepted/completed original sources may return its observers for owner cleanup; unknown effects/restoration/mapping/native issue remain nonreturnable. Expose this distinction to Scene through narrow original observations, not a caller-settable success boolean.

## Narrow production write set and owner handoff

| Owner | Files / actual change |
| --- | --- |
| D | `native_height_range.h`: capture provenance, general graph/worker/hidden members, exact join/settled state; separate sticky business error from mechanical prefix retirement. `rendering_device.h/.cpp`: proposed private entry points, finite prepare, exact enumeration/worker locks, generalized seal and actual issue/retain/recycle hooks. `gdextension_qualification_rd.cpp` plus existing scope callbacks only where needed to multiplex original hidden/native admission; preserve old ticket behavior. No public5 ABI change is required for this mechanical seam. |
| Scene | Latest original `height_capture_checked_sink.h/.cpp`: narrow RD friendship/association and mandatory per-operation enrollment, independent cleanup enrollment, retain original range/owner. Latest `height_frame.h/.cpp`: retain this same range alongside original prefix/resources; call prepare before actual checked render and close after finish/unwind for both success and failure. Do not erase a failed frame/prefix on failure. |
| Scene | Original Mobile/Clustered `render_particle_collider_heightfield_checked` and its original owner caller: consume the identical owner Ref into the actual quoted-target sink entry, execute geometry through existing checked calls, and retain all geometry/material leases. Existing fixed HeightFrame call still passes only framebuffer RID, and renderer begins plain sink.begin; these real callers need adoption. |
| Scene/owner | Attach original producer-close and each actual reader's last-use range/dependency before publishing/dropping the frame; retain the same owner until those obligations and prefix observers settle. Only then resume original `HeightFrameResources::retire_step` FB→depth cursor. D supplies mechanical original native facts; Scene supplies source occurrence/generation/shape semantics and actual reader membership. |

The current public5 no-adapter result remains correct until that same original owner/caller is integrated. No second owner, second native pair, second retirement cursor, synthetic source issuer, C budget refund or invented Complete follows from this seam.
