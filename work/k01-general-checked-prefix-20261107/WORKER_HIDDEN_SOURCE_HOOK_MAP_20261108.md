# Original worker / hidden source join: implementation hook map

Read-only production-source audit by default Sol, 2026-11-08. This records actual existing hook points and required state deltas for the already authorized K01 implementation; it does not install APIs or assert geometry completion. Concurrent callback edits may move line numbers; function names below are authoritative anchors. No build, GPU execution, production edit, or external message was performed by this audit.

## Existing exact original hooks

| Original source / approximate line | Required addition at the actual call |
| --- | --- |
| `rendering_device.cpp::_check_transfer_worker_operation`, 9641 | After successful original `thread_mutex` then `operations_mutex` try-locks, before changing `transfer_worker_operation_used_by_draw`, call the height dependency registrar with the exact `index, operation` and active original checked-call capture. This is before buffer/texture/array wrappers clear their resource markers. Refusal returns without publishing a dependency. The copied sink Transfer slice after the call is verification/accounting only; it cannot close this lifetime gap. |
| `_submit_transfer_worker`, 9462 | Its caller owns thread lock; optionally acquire operations lock as the current function already does for production. Snapshot exact `first=operations_processed+1,last=operations_counter` before actual driver submit. Following actual report, preserve the existing zero-prefix `ERR_BUSY && calls.empty()` early return. Otherwise join that original report to only watchers whose exact operation is covered, before the poisoned-report return and before any original source owner can recycle. The successful path's assignment of `operations_submitted` must use the same frozen `last`, not a later counter. |
| `_production_join_transfer_worker`, 9526 | Reuse its location/pattern for retry-retain of the still-owned exact report, but do not adopt its Registry or ProductionRequest identity. Keep source attachment idempotent; retries must never call submit. A separate height helper belongs beside this call in both aggregate submit paths and wait. |
| `_wait_for_transfer_worker`, 9562 | Resolve/join/retain all height watchers before original fence polling/recycle, pool reset, staging reuse, and `operations_processed=operations_submitted`. A busy range retain returns false with the worker and exact report intact. A simple pending flag checked at entry without a resume helper deadlocks retention; retry must actually attempt the pending retains. |
| `_acquire_transfer_worker`, 9256 / 9277 | Existing `qualification_batch_sealed` excludes workers from append/reuse. Add an independent height seal predicate at both same-thread and candidate selections, so the quoted current batch cannot grow after enrollment. Do not overload qualification credit ownership. |
| `_submit_transfer_workers`, 9734 / 9850 | Preserve pool→thread→operations ordering and existing nonblocking lock collection. In both bounded and ordinary loops, call the height join/retain helper beside production join. Ordinary currently ignores the production helper result; height errors must be explicitly propagated, not discarded. |
| `_free_transfer_workers`, 9905 | Existing submitted/poisoned refusal is insufficient if a watcher exists for an ended pending batch. Keep every watched worker alive until the original dependency is processed or exact sources are joined and independently retained. Never free a pool containing unresolved watchers. |
| `_native_height_seal_frame`, 10527 | Current all-members-equal-first-slot assertion is valid for standalone exclusive clear only. Preserve it for that recipe; geometry members must each map exact handle→own slot/buffer. Source membership remains matched by original frame epoch, slot and command in both actual issue loops. |

## Range and worker state required before first effect

Add finite range dependency rows with `{worker_index, required_operation, original_frame, original_epoch, state}`. States distinguish `AlreadyProcessed`, `PendingBatch`, `IssuedSourcesAttached`, `SourcesRetained`; unknown/error remains owned. Source edges need a tagged owner (`GraphMember`, `WorkerDependency`, `HiddenDependency`) rather than interpreting all member indices as graph rows. Retain/poll/return still operate once per unique immutable original native fact. Preserve original serial, queue, completion serial, issue, error, native code and completion capability consistency on deduplication.

Worker watchers retain the same range Ref plus dependency-row index, never the sink pointer after synchronous capture. Original thread→operations locks serialize watcher registration and report join with append/recycle. Under both locks:

- `required_operation <= operations_processed`: record original already-processed proof, add no invented historical serial.
- Current recording/ended pending batch: use existing `driver->qualification_worker_source_quote(command_buffer,prefix,future)` (Metal3 implementation at 242) to obtain actual already-issued wrapper facts and future bound, reserve capacity, then seal the exact original batch. Do not treat the future count as issued facts.
- Submitted unresolved batch: require exact covering operation interval and still-owned current report; attach/retain before releasing locks. No later `last_submission` may satisfy an older unregistered operation.

Registration requires active original checked-call provenance; a caller-submitted raw worker/operation pair cannot create membership. The RD callback scope must select the range before the real checked function and clear it after enrolling that actual result, including failure. Prevent concurrent second captures from replacing it; worker threads use watcher state under their original locks, not render-thread ambient state.

## Capacity facts and reservations

`_native_height_range_prepare` still derives capacity for one Begin+End clear with one MAIN_SLOT bound. That capacity cannot support geometry by merely copying its Ref. Reserve fixed storage from the sink's finite business count B, transfer count T and independent cleanup slot before any append. A safe graph-row upper bound is `(B+1)*max(5,1+DrawAppendResult::PREFIX_CAPACITY)`; checked arithmetic must reject overflow. This deliberately overbounds tagged operation rows and covers cleanup separately.

Dependency rows must cover every original resource dependency check, not only returned VA/IA Transfer records. Uniform-set dependency processing at draw can invoke checks without adding a `DrawAppendResult::transfer`. Either derive that finite bound from the actual retained uniform-set/resource arrays before the draw or add an internal per-operation preflight that reserves those rows before dependency processing. `T+B` alone is not a proven bound for those paths.

For each exact worker batch, reserve `prefix.calls.size()+future` using the original locked quote. For geometry graph members reserve the supported `native_issue_bound(MAIN_SLOT,1)` per possible exact slot, with slots bounded by graph rows. Hidden needs its own bounded original association (below). Reserve source and tagged-edge capacity before native issue, enforce overflow and truthful backend bounds, and mark unexpected post-effect excess uncaptured. Reservations count only fresh serials; reuse/cancel cannot release somebody else's credit. Existing Metal3 bounds are MAIN command+2 and worker command+1, PREPARE_SEGMENT=1 (`metal3.cpp:219`); Vulkan PREPARE_SEGMENT=0 is supported no-effect (`vulkan.h:405`).

Do not resize range arrays or watcher arrays after an original checked/native effect. Watcher storage may be preallocated per worker to the established maximum active range count and finite per-range dependencies; exceeding it refuses registration before consumption. Retention `ERR_BUSY` keeps reservations and original source owner intact.

## Reuse original admission routing; do not stack a replacement issuer

`RenderingDevice::QualificationSourceAdmissionScope` (`rendering_device.h:365`) is already installed around uniform creation, worker submit, maintenance, end-frame and begin-frame. Its callbacks are in `gdextension_qualification_rd.cpp:367–430`. `RDD::NativeSourceAdmissionScope` replaces a thread-local pointer; installing a parallel height scope on top would mask qualification/production observation.

Extend these existing callbacks to multiplex height mechanically:

1. Height preflight validates finite space for the selected original worker watchers / active checked range, without mutations.
2. Existing qualification admission performs its unchanged canonical checks.
3. Commit fresh height reservations only after both accepted; cancellation rolls back only serials reserved by that exact call and not attached to actual facts.
4. `capture_hidden` retains the existing Frame observer transfer and ticket/production calls, then separately joins/retains selected height facts. A height retain refusal does not return false after Frame has accepted ownership: `_copy_queue_flush` interprets false as permission to release the transferred observer. Preserve report/owner and communicate pending retention through original RD state.

`can_block` currently forbids blocking with Registry ownership or a native-disposal slice. Keep those rules. Height does not need a Registry issuer or E09 identity.

## Hidden source association cannot be recovered from Frame history

Original `_copy_queue_flush` (`metal.cpp:3147`) creates a real report via `qualification_prepare_segment` and immediately invokes `capture_hidden_native_source`; false releases its one transferred observer. This is the exact auto-flush observation hook. It must associate only the actual selected active original operation / pre-registered pending-copy dependency, with a separate retained range observer.

Explicit prepare calls at RD.cpp ~11459, 11520 and 13621 bypass the capture callback and push directly into `Frame::qualification_hidden_reports`; they need the same shared join helper with their original report and pre-bound pending-copy membership. A scope alone does not cover these direct returned reports.

For a hidden copy still pending when a checked call returns, preserve membership against the original backend pending copy batch before the call scope ends. The current backend interface does not expose an authenticated pending-copy batch association. Add it at the original Metal copy encoder/batch lifecycle (same real CompletionRecord path), or flush that exact dependency through existing prepare while the original selected-call association and pre-effect reservation still exist. Do not attach every end-frame hidden report to every range, and do not label unrelated existing Frame history as exact membership. A selected call that did not generate/use a hidden dependency must add no hidden row.

## Verification controls tied to these hooks

Required CPU controls should drive actual original helper paths with test driver counters: processed dependency adds no source; pending batch seal blocks append; wrong operation interval does not join; submitted dependency retains before lock release; zero-prefix BUSY retries submit once when admitted; partial issue joins before poison; retain BUSY retries retention without second submit/recycle; two ranges sharing a worker retain/release independently; worker source quote overflow refuses before native call; hidden Frame acceptance plus range retain BUSY preserves the transferred Frame observer; unrelated hidden source is not attached; explicit prepare report and auto-flush callback both enroll selected membership; late frame/epoch fails before effect. Current synthetic clear tests and object compilation do not establish these branches or producer/reader closure.
