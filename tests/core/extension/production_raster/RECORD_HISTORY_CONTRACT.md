# Production record history capability

Status: implemented; append/native distinction passed the full editor build and CPU runtime verification on 2026-11-06. See RECORD_HISTORY_RECEIPT.md. No GPU acceptance claimed.

This contract adds an independent, versioned query for the original Production request/control history. It does not infer execution from an append prefix, graph count, error code, output RID, a qualification ticket, or a caller's copy of a report. It grants no recording, cleanup, resource, or logical Runtime authority.

## Identity and lifetime

The query takes `GDExtensionProductionRequestRefV1 request`, `uint64_t control_serial`, and an output `GDExtensionProductionRecordHistoryV1 *`. Registry authenticates the current original owner/attachment/library generation/device association through `production_request_locked(..., false, ...)`, then compares the exact current control serial, control input request serial and record generation. The control must be RECORD. A matching serial in another request is never sufficient.

The original control cell retains its history through PENDING, QUEUED, EXECUTING, RETURNED and OWNER_CONSUMED. Consuming a RECORD result changes only owner-consumed state; it does not erase the returned history. Accepting the next control of any kind invalidates the previous query identity. Failed attempts to accept a replacement do not invalidate it. Request release invalidates all its history; arena-slot reuse cannot revive it.

No persistent archive or additional per-control allocation is required. The request retains distinct cumulative native-effect and append-entry latches; the existing fixed control cell retains their per-control history snapshot. Reading history neither consumes the output nor schedules service.

## Proposed public ABI

`device_session_production_record_history_v1(request, control_serial, out_history)` returns `GDExtensionDeviceSessionResultV1`. It zeroes a validated, aligned output before lookup. The DTO is meaningful only on query success; a query error is never a no-effect certificate.

Suggested `GDExtensionProductionRecordHistoryV1` fields:

| Field | Type | Meaning |
| --- | --- | --- |
| abi | GDExtensionDeviceExitAbi | Version 1, containing DTO size, reserved zero. |
| request_serial | uint64_t | Exact live request identity. |
| control_serial | uint64_t | Exact RECORD control identity. |
| record_generation | uint64_t | Original request generation. |
| state | uint32_t | Versioned public PENDING/QUEUED/EXECUTING/RETURNED/OWNER_CONSUMED enum; do not expose private enum ordinals implicitly. |
| flags | uint32_t | Facts listed below; reserved bits zero. |
| last_schedule_result | GDExtensionDeviceSessionResultV1 | Latest completed publication attempt, meaningful only with SCHEDULE_RESULT_VALID. |
| record_result | GDExtensionDeviceSessionResultV1 | Immutable actual control result, meaningful only with RETURNED. |

Flags:

- `SCHEDULE_ATTEMPTED`: this exact control reached the FIFO publication attempt.
- `QUEUE_POSSIBLE`: a publication may have admitted this exact control; conservative until a definitive failed admission or exact return proves otherwise.
- `ADMITTED`: the original FIFO returned successful admission for this exact control. This is historical and remains true after dispatch/return/consume.
- `SCHEDULE_RESULT_VALID`: `last_schedule_result` is an actual completed publication result.
- `CONTROL_EFFECT_ENTERED`: this exact control crossed a native-effect attempt boundary at least once.
- `REQUEST_EFFECT_ENTERED`: this original request crossed such a boundary at least once, including an earlier control.
- `RETURNED`: the exact control has published its immutable return; remains true after consume.
- `OWNER_CONSUMED`: the original owner has consumed that return.
- `TERMINAL_NO_EFFECT`: RETURNED and neither native-effect latch has ever been set for this original request/control, with conservative unresolved-state guards.
- `CONTROL_APPEND_ENTERED`: this exact control reached an original append-capable attempt. Resource allocation/preparation/retirement alone does not set it.
- `REQUEST_APPEND_ENTERED`: this original request reached an append-capable attempt in this or an earlier control; never reset by consume or a later empty RECORD.
- `TERMINAL_NO_APPEND`: exact RETURNED with neither append latch set and no capture in flight, open list or recorded graph/native facts. Native allocation/cleanup latches may remain true. This fact does not certify physical return or erase retained obligations.

The cumulative request latch is intentional: a later empty/rejected RECORD must not certify a request as effect-free after an earlier control already attempted native work. If a consumer only needs per-control history, it may inspect CONTROL_EFFECT_ENTERED; it must not reinterpret that alone as a whole-request refund certificate.

## Publication transitions

1. Accepting RECORD initializes fresh control history with the exact identities, PENDING, no schedule result, no admission and no control effect. It copies/observes the cumulative request effect latch without clearing it. Failure before control acceptance yields no new queryable control.
2. Before invoking `_try_production_control_dispatch`, set SCHEDULE_ATTEMPTED and conservatively set QUEUE_POSSIBLE while holding the original Registry mutex. This must precede any publication that another thread could consume. Do not set ADMITTED before the FIFO reports success.
3. If `_try_production_control_dispatch` returns false, its documented all-or-nothing publication contract proves this attempt not admitted. Restore PENDING; clear QUEUE_POSSIBLE for this still-unadmitted control; leave ADMITTED false; record ERR_BUSY as the completed schedule result. Return the real failure to the caller. This is a retryable refusal, not RETURNED and not TERMINAL_NO_EFFECT. If the queue API cannot prove non-publication, keep QUEUE_POSSIBLE true and refuse any no-effect inference instead.
4. A successful publication sets ADMITTED and QUEUED. Keep QUEUE_POSSIBLE true until this exact control returns. Repeated schedule calls for an already queued/executing/returned cell do not publish again or reset history.
5. The implemented missing-server/non-singleton branch records ERR_UNAVAILABLE when it has already resolved a valid exact control. Authentication, metadata-entry, freeze or other earlier context/admission refusals do not promise to update last_schedule_result; callers use their immediate error. Only actual publication attempts and that missing-server branch record schedule results. Authentication failures cannot mutate another control's history.
6. Dispatch authenticates registry, attachment, request and control scalars before changing state to EXECUTING. A validated queued control rejected by dispatch preflight still publishes an exact RETURNED result. A command that cannot identify a live matching control cannot manufacture a return certificate.
7. Return publication clears QUEUE_POSSIBLE, sets RETURNED and stores the actual immutable result. ADMITTED remains historical. TERMINAL_NO_EFFECT is derived under the same lock from RETURNED and both effect latches; it is never set merely because execution was rejected or no graph was captured.

The existing publication path holds Registry mutex across `_try_production_control_dispatch`, and CQMT does not execute inline. These are implementation requirements, not assumptions a consumer may independently reconstruct from wall-clock timing.

## Source contract and append/native separation

The source contracts are `contracts/device-exit/SOURCE_ACCOUNT_IMPLEMENTATION.md` (required order, lines 110 onward) and `contracts/device-exit/R01_R02_PRODUCTION_JOIN.md` (original bridge call order, lines 257 onward) in the external ANGEParticleDesign docs main copy. They require authenticated capture and fully reserved Core child **without append**, followed by original B marking AppendStarted/possible-effect before A activates that same request. This history query cannot move that Runtime transition after activation or authorize resetting a consumed once.

Resource preparation can allocate native objects and incur mandatory cleanup while remaining before business append. STORAGE_CREATE, SET_CREATE, output-owned buffer/set/pipeline factories and native disposal/maintenance therefore set only native-effect history. A preparation-only request may report TERMINAL_NO_APPEND while REQUEST_EFFECT_ENTERED is true: the former describes recording history, the latter preserves physical obligations. It must drain/release those physical owners through their original mechanisms.

The separate append helper marks both native and append history immediately before typed upload/copy/compute/readback/raster record attempts, including potential partial prefixes and list closure. A validated real action graph also conservatively records append entry. Native effect alone cannot be interpreted as AppendStarted. Conversely, an empty prefix or later rejection after an append-capable attempt cannot produce TERMINAL_NO_APPEND. Queue admission/possible publication remains conservative activation history, distinct from observed native entry: even a final physical no-append report does not authorize rollback of B's already performed AppendStarted/once transition.

## Native effect boundary

`effect_entered` means an attempt may have caused a native or graph effect. It is deliberately more conservative than known effect or known accepted native issue.

All owner/context/thread/resource/capacity/revision preflight checks run first. Immediately before releasing Registry protection to make the first native-effect-capable call, atomically under Registry lock set both CONTROL_EFFECT_ENTERED and REQUEST_EFFECT_ENTERED. The latches never clear during the lifetime of their respective identities, including when the native call returns a rejection, an empty prefix, an invalid handle or an error.

Concrete native-effect insertion sites (append-capable sites additionally set the separate append latches):

- `production_record_effect_locked`: before `_record_source`/native upload/copy/dispatch effects; a return from a pure preflight must leave the latch false. Account for transfer-worker preparation capable of native effects before the nominal action append.
- `production_raster_begin`: after exact owned-resource and capacity checks, before `_draw_list_begin_recorded`.
- `production_raster_close_locked`: before `_draw_list_end_recorded` if closing can itself enter effects; do not assume a preceding begin was observed correctly.
- `production_close_lists_locked`: before `_compute_list_end_recorded`, including frame-return closure.
- Any native factory/output-owned creation reached inside an original RECORD callback: latch before the factory attempt. Existing output-owned paths must not bypass history merely because they do not use `production_append_v2`.

A closure reached under CANCEL/OBSERVE may update the cumulative request latch but must not write a RECORD history for a different control. Querying a consumed RECORD while no newer control exists remains read-only and stable. Preparing a resource in another control before RECORD must likewise make REQUEST_EFFECT_ENTERED true if the intended certificate covers the whole request. This wider audit is required before exposing TERMINAL_NO_EFFECT as a whole-request fact.

Do not set the latch at record callback entry, before pure authentication/preflight, at schedule acceptance, or based on graph/native counters after the call. Do not clear it when a prefix is empty, a native result reports no new effect, work is cancelled, resources return, or output consumption completes.

## Internal storage and API suggestions

Use a trivially copyable internal `ProductionRecordHistory` member of `ProductionControlCell` with `schedule_attempted`, `queue_possible`, `admitted`, `schedule_result_valid`, `control_effect_entered`, `control_append_entered`, and the retained schedule result. RETURNED/OWNER_CONSUMED and the record result project the original control state/output, including after consume; no competing return-state copy is needed. Reuse the original cell identities. ProductionRequest retains `native_effect_entered` and `append_effect_entered` as separate monotonic cumulative latches.

Centralize mutation in private Registry helpers:

- `production_record_history_schedule_attempt_locked(request)` and `...schedule_return_locked(request, admitted, result)` for the original queue publication only.
- `production_native_effect_enter_locked(request)` validates the current executing control and updates the request latch; it updates control history only when that control is RECORD. A caller must hold the original capture/control retention before entering native work.
- `production_append_effect_enter_locked(request)` calls the native helper and sets the request append latch, plus the current RECORD control append latch when executing. It is not called by resource factories or preparation cleanup.
- Exact returned status is projected from the original control state on every return path, including dispatch preflight rejection.
- `production_record_history(ref, control_serial, out)` only authenticates and projects history under Registry mutex. It performs no RD work, polling, implicit consume or new admission.

Do not add a second owner, side queue, timer, unbounded history vector, prefix classifier or public setter. Initialization sites that currently assign `cell = {}` are the exact replacement boundary and must preserve the separate cumulative request latch.

## Required behavioral tests

| Trigger | Independent expectation |
| --- | --- |
| Bad/foreign request or wrong control | Query fails, output zero; no history mutation. |
| Reserve succeeds but RECORD acceptance/freeze fails | No new accepted control and no terminal certificate. |
| Known FIFO capacity refusal | Same control stays PENDING, ADMITTED false, QUEUE_POSSIBLE false, real ERR_BUSY recorded; no terminal certificate. |
| FIFO success before dispatch | Exact control QUEUED, ADMITTED true, QUEUE_POSSIBLE true; no terminal certificate. |
| Callback returns without reaching a native attempt | Exact RETURNED, no effects, TERMINAL_NO_EFFECT true only if request has never attempted effects. |
| Native append rejects after entry boundary, including empty prefix | Effect latches true; TERMINAL_NO_EFFECT false. |
| Frame-return close fails after a native attempt | History remains effect-entered; request effects_pending remains true independently. |
| Dispatch rejects the exact admitted RECORD before callback | Exact returned error; no-effect only if both latches remain false. |
| Consume exact returned RECORD | History remains queryable, RETURNED and OWNER_CONSUMED true. |
| Next control accepted | Old control query fails even if next control has no effects. |
| Replacement acceptance refused | Prior consumed history remains queryable. |
| Earlier request control allocated a resource; later RECORD is empty | REQUEST_EFFECT_ENTERED true; TERMINAL_NO_EFFECT false; append flags false and TERMINAL_NO_APPEND true after exact return. Physical cleanup remains required. |
| Output-owned factory succeeds or rejects after entry, without business append | Native latches true; append latches false; exact returned RECORD may certify no-append, never no-native-effect. |
| Partial upload, compute/readback or raster append attempt followed by empty/rejected prefix | Both native and append latches true; neither terminal no-effect nor terminal no-append. |
| Earlier RECORD attempted append; later RECORD is empty | Request append history persists; later control's append flag may be false, but TERMINAL_NO_APPEND remains false. |
| Concurrent schedule/query/dispatch | No observed state claims terminal no-effect while a publication or native attempt can still occur for the same control. |

Native/GPU tests remain separate from metadata transition tests. A passing headless state-machine test cannot establish actual raster creation, drawing, source completion or driver-free acceptance.
