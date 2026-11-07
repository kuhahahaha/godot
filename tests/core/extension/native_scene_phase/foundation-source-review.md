# K06 typed relay and original-operation ABI review

Source review only. No build, runtime test, active-source edit or git operation was performed. This reviews the proposed minimal integration, not a completed patch. It follows `IMPLEMENTATION_OWNERSHIP.md` from `/Users/macbookair/Documents/Codex/2026-10-15/xu-y/scene57-source-api-contract-20261107` and canonical K06 sections 2, 4, 5 and 6.

## Decision and concrete implementation shape

A once-installed typed receiver leaf in the **existing CallableCustomExtension** is a suitable minimal choice. Keep its existing userdata, owner RecordRef, CleanupAction, cleanup_serial and adopted lifetime. Keep the existing NormalRelay Callable, original holder serial, library generation, destruction thread and active_deliveries. Keep Scene sink configuration and its relay token on the existing ProductionAttachment. This adds a typed invocation path to the existing owner; it must not add a separate DSO-function registry, arbitrary dispatcher, callback ownership chain or once table.

Implement the path as follows:

1. Install the typed leaf only on an adopted original CallableCustomExtension while its owner generation is live and the caller is authenticated to that owner. Inspect core metadata only. Reject a different second installation; an exact duplicate may return the same original association. Use the existing userdata rather than another unmanaged userdata/destructor pair. A token obtained by calling extension code, custom equality/hash or is_valid is not admission.
2. Registration resolves the actual original NormalRelay token and validates **same attachment library record and generation**, live device session/epoch, exact ABI header, phase enum and bounded max_operations. Match the actual holder identity, not merely a nonzero token or matching numeric generation. Duplicate same relay/device/phase returns its existing registration and cannot create an additional dispatch.
3. On real SceneBeforeParticles dispatch, reserve active_deliveries under the original Registry mutex before borrowing the CallableCustomExtension pointer. Apply the same cancelled/releasing/live-generation gates as call_normal_relay. Do not copy a Callable to the RT stack: its last reference could destroy the holder on the wrong thread. The reservation pins the original relay callback until return.
4. Enter through original `production_record_receive_locked` CallbackScope and physical frame. Invoke the typed leaf **without entering another CallbackScope**. Validate that its owner is the already-admitted current owner and the same attachment/library generation. `production_physical_request_locked` requires `RegistrationContext::current == frame->protected_entry` and the exact frame serial; ordinary Callable::call would create a nested frame and break this identity. This must be a private already-protected invocation path, not a public unchecked typed call.
5. Preserve the original RECORD return path: close lists/segment, retain accepted prefix facts and return the original mailbox report. A failed typed entry must still release active_deliveries and unwind the original frame. Do not leave a callback-frame pointer or borrowed stack POD after return. Actual phase expiry encloses the callback window; expiry never frees an accepted read's retained content.
6. Unregister first closes new dispatch, then uses existing cancel/release/observe/forget lifetime operations. Release the Callable only on `destruction_thread`; active deliveries or release-in-progress are Busy. `holder_closed` is set only by `unlink_holder_locked` for the exact holder/record/generation, not by dropping the registry Callable copy or setting a registration flag. Retain attachment configuration until original active calls/control custody return. Close must be serviceable without another draw.

These steps preserve the original normal-entry and unload barriers. A holder that also retains unrelated external references can legitimately delay final holder acknowledgement; unregister must not claim closure early or silently revoke another consumer's holder ownership.

## Exact current source support

- `core/extension/gdextension_interface.cpp:48`: CallableCustomExtension already owns RecordRef, CleanupAction and cleanup_serial; constructors enroll its original cleanup. Its destructor uses CleanupScope and actual free_func return.
- Same file around :331: register_normal_relay dynamically checks the original custom type, adopted state, valid owner and call_func; no extension callback participates in admission.
- `gdextension_library_exit_guard.cpp:469–602`: existing register/call/cancel/release/observe/forget lifecycle, including active_deliveries and exact destruction thread.
- Same file :169–184: unlink_holder_locked acknowledges only the original holder identity.
- `gdextension_production_window.cpp:521–584`: original CallbackScope and ProductionPhysicalFrame around RECORD, followed by actual list/segment closure.
- Same file :681–700: exact current-frame, library holder, device epoch, control and RT authentication. Typed dispatch must preserve these checks.

## ABI reconciliation

K06's latest explicit POD is the governing shape: AbiHeader is 16 bytes, ScenePhaseSinkConfig is 56 bytes, NativeScenePhasePOD is 128 bytes under its specified fixed fields. Reuse `GDExtensionDeviceExitAbi` for version/size/reserved (same u32/u32/u64 layout), and introduce fixed C DTO names under the existing interface conventions. No C++ Callable, reference, container or std::function crosses the boundary; receiver parameters are const POD pointers borrowed for the call.

Current Core identities are **not already K06's 32-byte opaque ABI**:

- Internal `NormalRelayToken` is two u64 and explicitly documented as not the planned exported relay ABI.
- OriginalDeviceSessionRef and ProductionPhysicalContextRefV1 are also 16-byte typed refs.
- K06 phase_token/device_capability/physical_context are Opaque32 fields.

Therefore retain one explicit owner-validated opaque encoding/decoding at the existing ABI edge. Do not reinterpret 16 bytes as 32, treat all-zero padding as authority, or pass C++ internal tokens directly. The encoding only locates existing owner state: validation still resolves original registry/session/frame and domain. Fixed declared sizes, reserved-zero checks and cross-kind refusals must be tested. The final callable package must include the actual exported relay/registration/close DTO declarations; internal NormalRelayToken alone is not a completed K03 opaque ABI.

## OriginalOperationKey: actual reuse, exact missing projection, and implementation seam

It is **not exact** to construct K06's key from current `request.operation + action ordinal` alone. `GDExtensionProductionOperationV1` has only owner_request_nonce, record_generation, canonical_begin and canonical_end. The action contributes canonical_suboperation/source_recipe_substep/ordinal. None exports original E09 operation_ref, U04 cursor_ref or schedule_digest. Library generation, record generation and U04 owner generation are distinct and cannot substitute for one another.

There is a concrete existing owner chain to extend; no new identity registry is needed:

| Required source | Actual retained owner / seam |
| --- | --- |
| E09 operation identity | `E09NativeRecorder::FullState::join` retains `FullRecordingJoin`; `join.operation()` is the original `RuntimeFullOperationRef` retaining AcceptedStepState. |
| Original U04 cursor | `join.accepted().settlement_cursor()` returns `SettlementRecordCursor`, issued by the original SettlementRecordSink; its weak owner/slot/incarnation are private, with same_owner comparison. It currently has no byte exporter. |
| Original schedule digest | `join.mapping()` retains the actual C06 finalized mapping; `C06SystemMappingRecords::schedule_digest` is already present. Use that owner projection, not a recomputed action-table hash. |
| Original record request/generation | `RecordSession::issue_full_record_provenance(reservation, join)` currently returns request_nonce and RetirementProvenance generation/range; E09NativeRecorder constructs OperationV1 from this at its original reserve path. |
| Actual append/once evidence | Existing D request action/graph/native facts and original accepted append, retained even before owner report consumption. Do not use observed U04 cursor movement as acceptance evidence. |

Source root inspected: `/Users/macbookair/.codex/worktrees/e25-e26-final-integration/ANGE/src`. Relevant files: `godot/particle/e09_native_recorder.cpp` FullState around :547 and provenance export around :901; `particle/stage_execution/full_recording_join.h:279`; `particle/runtime/full_recording_operation.h:14`; `particle/runtime/instance_attachment.h:153`; `particle/runtime/settlement_record_cursor.h`; `particle/stage_plan/stage_plan_static_mapping.h`.

**Minimal owner postimage:** extend the original RecordSession/FullState preparation/export point to produce a fixed immutable Scene operation binding from these retained owner objects. The original cursor owner supplies a stable opaque reference with its original incarnation validation; the original operation owner supplies its own opaque reference; copy the existing finalized schedule digest. Install the resulting binding once on the original prepared D request, authenticated against that request's existing provenance and attachment owner. Keep strong operation/source ownership in the existing FullState/retirement reservation; the POD is only a transport projection. An identical retry reuses the original binding; replacement or mismatched provenance is rejected before native work.

This is an additional narrow **export/accessor on existing owners**, not a caller-chosen bag of bytes and not a second lookup registry. In particular, adding three Opaque32 fields that any caller can fill is insufficient to establish the original association. The actual B/Runtime exporter and D install validation need to ship together for Scene production acceptance. Core phase infrastructure can be implemented independently while that narrow join remains unavailable.

At append, validate the installed binding and active phase/sample association against the original request/action. Atomic acceptance consumes only that phase permit and attaches retained read custody to the original append fact. A new phase with unissued original work may receive a new attempt; accepted work returns the original fact; unknown work follows existing failure retirement. Do not add a boolean phase-side once cache. Both K06 delayed-observation sequences must be tested before claiming this join complete.

## Required verification boundaries

The eventual candidate should demonstrate real hook-only phase issuance, no ordinary FIFO grant, expiry at callback return, mismatched holder/generation/device refusals, active-delivery close and actual holder acknowledgement, absence of nested scope mismatch, bounded budget preservation of the same original mailbox/cursor, and the two cross-phase once sequences. Sphere/Height content and D32 read custody remain with the Scene owner; none of this issues late-view R32 CurrentDepth.

Review conclusion: the original-holder typed leaf is the correct minimal carrier provided the explicit lifecycle checks above are preserved. Current physical OperationV1 is insufficient for the full K06 key, but its original E09 FullState/RecordSession/Runtime operation/cursor/finalized mapping chain provides a concrete extension seam. No alternate dispatcher or once registry is justified.

## In-progress candidate follow-up (not final frozen review)

Read the isolation candidate after its first patch generation; author is still revising it. Sent these concrete findings directly to the author:

1. **Release path deadlock:** its new scene-content return loop was placed after existing `if (!reclaim && native_issued) return OK`, while the new loop refused `reclaim` and missing budget/usage. Actual native-read RT release therefore skipped the loop, and owner reclaim always refused it. An unissued retained borrow also caused the public release preflight (`reclaim=false`, no budget/usage) to return Busy before it could schedule a RELEASE. Required repair: pure readiness preflight must allow the original RELEASE to be scheduled; the actual bounded RT RELEASE phase returns content after native completion guards; final owner reclaim requires already-returned content and never calls the source owner. No state-forcing fix is acceptable.
2. **Once-install race:** typed leaf installation checked/wrote CallableCustomExtension fields outside Registry mutex around register_normal_relay. Two admitted same-owner threads could both observe an uninstalled leaf, create two original relays and overwrite the leaf/token. Admission is not mutual exclusion. Require atomic check/install under the original owner lock or an enforced original single-thread installation rule with locked verification. Never publish a new side registry to fix this.
3. **Callback expiry identity:** phase token used `(instance, registration, occurrence, epoch)` and expiry_serial equaled occurrence. Multiple original RECORD callbacks in one real scene occurrence therefore shared identical phase tokens although K06 expires each at callback return. Include the original protected frame serial in token/expiry identity, and resolve against the current original frame; device epoch is already bound by the original capability/owner. A different physical_context alone does not ensure a source sample API rejects the old phase token.

These findings are source-derived and need verification against the author's final regenerated patch. They do not change the recommended original-holder architecture.

Final source recheck of author's regenerated foundation: multi-mailbox waiting scan now precedes cached close acknowledgement and returns Busy after scheduling one original mailbox; the source-content return holds a temporary Ref across unlocked return/destruction and capture_inflight remains held until it returns. Original three fixes were also rechecked. No additional definite blocker identified for freezing the foundation candidate. This is not compilation/runtime approval; operation binding remains ERR_UNAVAILABLE pending the actual owner exporter, and multi-mailbox/no-draw/active-holder close still requires execution.
