# Frozen same-ticket multi-readback interface for A/B/C adaptation

2026-11-01 (Asia/Shanghai). Native writer/builder: D, `/Users/macbookair/Documents/Work/godot_ANGE`, `codex/particle-mainline`, base `98d315818db624e4b81f867f718ec5fac9beb385` plus recorded dirty files. This supersedes the earlier additive range-query sketch in this work file. Implementation, tests=yes build and standalone native Metal controls have passed; ANGE integration/acceptance remain pending. Fields and symbol routing below are implemented.

Source contract: A `work/integration-current-20261101-r4/METHOD_NATIVE_CONTRACT.md`, corrected by root: range is 48 bytes and observation 56, without invented padding. Native adds the required second uniform set per phase. Numeric=1 range, Method=3 ranges, total <=16384; exact ordered gapless result partition, unused slots zero. Readback roles remain distinct from upload roles.

## Compatibility and symbols

Keep current V1 structs, exported functions and QRT kinds 1–9 byte-compatible. Their pointers have no output capacity or whole-struct size, so silently replacing RecordV1/ReportV1 can overrun an old caller. The unified multi-range wire is explicitly V2, with `abi.abi_version=2`, `abi.struct_size=16`, `abi.reserved=0`. Existing PrepareV1 and all non-V2 request ABI fields stay version 1.

Add `GDExtensionQualificationReadbackRoleV1` with exact values: NUMERIC_RAW_OUTPUT=0, METHOD_RAW_RESULTS=1, METHOD_DESTINATION=2, METHOD_FAULT_PLANE=3, METHOD_RESULT_EPILOGUE=4, ROLE_COUNT=5. Constants use `GDEXTENSION_QUALIFICATION_READBACK_<NAME>_V1`.

`GDExtensionQualificationReadbackRangeV1` = `uint64_t readback_buffer, source_allocation_generation, observed_backing_serial, readback_offset, readback_size; uint32_t result_offset, role;` — size48 align8. `GDExtensionQualificationReadbackObservationV1` = RangeV1 source + uint64_t readback_request — size56 align8.

`GDExtensionQualificationRecordV2` keeps RecordV1's fields through body_set at the same offsets (0–232), then:

| offset | field |
| --- | --- |
| 240 | uint64_t capture_set1 |
| 248 | uint64_t body_set1 |
| 256 | uint32_t capture_set_count |
| 260 | uint32_t body_set_count |
| 264 | uint32_t readback_count |
| 268 | uint32_t readback_size (aggregate) |
| 272 | RangeV1 readbacks[3] |
| 416 | uint8_t capture_push[16] |
| 432 | uint8_t body_push[16] |

Size448 align8. Set0 remains capture_set/body_set. A pipeline with one set requires set1=0; a pipeline with two sets requires the original set1 RID and exact shader_set/format checks. No arbitrary set counts.

`GDExtensionQualificationReportV2` keeps ReportV1 fields through fact_count at offset76, then uint32_t readback_count at80, uint32_t readback_size at84, ObservationV1 readbacks[3] at88, uint32_t owned_size at256, uint32_t reserved at260, request_digest[32] at264, unit_digest[32] at296. Size328 align8; ticket-ref+report result344.

Added direct symbols (return existing DeviceSessionResultV1):

- `device_session_qualification_prepare_uploads_v2(bridge, const UploadPrepareV1 *request, const UploadPlanV1 *plans, uint32_t plan_count, TicketRefV1 *out_ticket, ReportV2 *out_report)`.
- `device_session_qualification_record_prepared_v2(bridge, ticket, const RecordV2 *record, ReportV2 *out_report)`.
- `device_session_qualification_observe_v2(bridge, ticket, const BudgetV1 *budget, ReportV2 *out_report)`.

Existing upload/facts/copy/cancel/release V1 functions operate on this original ticket. Existing V1 record/observe functions reject V2-mode tickets rather than truncate their observations. Each V2 prepare reserves all three original async-result serials before any upload effect; actual per-range allocations occur before recording.

QRT **kind10** is record_prepared_v2: request448, payload original TicketRefV1=16, result328, segments0. QRT **kind11** is prepare_uploads_v2: request152, payload existing original plans, result344 (TicketRef16+ReportV2), segments0. Configure maxima increase to request448/result344 for V2 callers; old caller maxima remain valid. Upload remains original kind8 with bounded bytes/digest/segments. No generic upload operation or new scheduler.

Append Method upload roles 8–17 in source declaration order: DOMAIN_COUNT, FAULT_PLANE, INPUT_GATE, INSTANCE_GATE, MAPPING, OUTPUT_GATE, RAW_PARAMETERS, RAW_RESULTS, RESULT_EPILOGUE, INVOCATION_INPUTS. Prefix is GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_, suffix _V1. METHOD_RESOURCE=18, ROLE_COUNT=19; logical resource family remains E23/E24; owner_kind authenticates the actual allocation issuer (E05 plus its nonzero original backing for the current Store path). Texture pixels stay typed producer owned.

## Behavior

Each used range retains its actual BufferGetDataResult, request serial and copy cursor. Append Capture and Body once, then actual copies. Capacity uses 2+sum(ceil(each_size/32)), plus original upload segments. ReportV2 exposes each original descriptor/request and aggregate immutable bytes; copy_v1 writes each actual result at its frozen result_offset only after every range completes and all native/graph/physical obligations return. No fabricated guards or CPU result padding.

Each same-graph readback copy must have Body→copy graph dependency. Later-frame copies require the original Body graph returned with exact successful native completion and no possible/unknown obligations, checked before admission. Failure or cancellation seals further append, retains the entire original prefix, and preserves first native cause. Release remains physical retirement and cannot become cancellation.

D has one builder. Active Reload source/tests are frozen for current tests=yes build. Texture/sampler preflight proxy edits only its named function/test after behavioral RED. Physics candidate remains isolated. After initial RED verification D integrates range implementation, reruns affected tests and real Metal; no ordinary GPUParticles picture is promoted to ANGE acceptance.


## Verified delivery checkpoint

The implementation is now in the actual D tree. SDK snapshots are `v2/gdextension_interface.json` and `v2/gdextension_interface.h`; `v2/ABI.json` includes hashes and the pinned monolithic executable. JSON SHA256 `135740c3c51c83171a96f8f1f912837eec59c04edd0a3c604039ce1141ba9c72`; header SHA256 `971a1ab90541900c50cde6eb45f62e7f5fcc1a95cbcb9a8632fab792a9835289`.

Fresh tests=yes build rc0, source drift empty. Focused 7 cases/456 assertions and qualification neighbors 184/3134 passed. Real ordinary Metal safe-thread V2 positive returns 3 distinct original async requests, 44 exact bytes, 11 known successful facts and physical acknowledgements. V2 uploaded-prefix cancellation returns 6 settled facts and no result. Recompiled V1 ticket and upload neighbors pass (5 and 6 facts). Full exact commands/results are under `work/native-multireadback-20261101/`. This checkpoint is a native API/control delivery, not a real ANGE request, Store/Texture family acceptance, separate-owner upload caller, typed-close, or G1 claim.


Store allocator correction also passed exact native RED→GREEN: logical METHOD_DESTINATION/METHOD_RESOURCE18 retains actual E05 backing authentication (instance, child serial, generation, physical cell, RID and live state). The old pinned host reached kind10 after successful prepare+4uploads and refused it; the corrected pinned host completed the exact three ranges and physical cleanup. The E24 issuer-tag/nonzero-E05-ref mismatch remains refused. Final native host/metadata are in `v2/ABI.json`; exact Store receipt is `work/native-multireadback-20261101/runtime/results/20261101-111020/results.json`.


V2 C06 empty-domain dispatch is supported without synthesizing a domain: sample_count0, exact canonical range0..0, body_groups_x1 and push envelope0. Capture and Body are still recorded; the actual shader guard returns all lanes and all original readbacks/physical obligations remain required. This V2-only admission flag reaches the common internal preflight; Numeric V1 keeps its original nonempty rule. No ABI layout/symbol change.
