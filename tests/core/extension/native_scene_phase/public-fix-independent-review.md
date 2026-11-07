# Independent source review: public boundary and RELEASE fix

Reviewed patch SHA256 `f17ce66f4f3cf6578a2976ed10dc9f4f0bcb4865e893d34a32e3936a6dc2cd68`, all four changed files, against current active foundation. No active edits, builds or tests by this reviewer. Root reports foundation r4 build exit0; public-negative RED/patch GREEN are not yet reviewer-observed results.

**Conclusion: no definite source blocker found in this patch.** It fixes actual public input aliasing/alignment, typed-holder admission and missing RT RELEASE budget plumbing while preserving the original ownership model. The checks below are limited to source; runtime acceptance remains pending.

## Holder metadata and publication

`scene_phase_callable_metadata` is a core-only metadata accessor adjacent to CallableCustomExtension. It checks custom/adopted/typed receiver and exact original relay token, then returns borrowed addresses of the original CleanupAction and RecordRef. Both new callers retain Registry mutex throughout accessor and immediate `scene_phase_holder_matches_locked` validation. The relay's original Callable remains retained, and release/forget need that same mutex; no borrowed pointer is stored after unlocking. Dynamic cast/is_custom/get_custom inspect core metadata, without calling custom hash/equality/is_valid or a DSO callback.

The original holder validator checks owner/reference Registry slot, holder reference, pending/disposal state, current live qualified record, holder generation and the exact relay holder serial/record/generation. Thus an ordinary normal relay with no installed typed leaf can no longer be exported/registered as a Scene receiver merely because its wire token has the right shape. Call-time exact custom.scene_phase_relay equality is an additional check, not a replacement for holder validation.

Installation is restricted by the pre-existing validator to the original destruction thread before reading the once-written leaf. The patch additionally publishes both typed fields under Registry mutex, matching the cross-thread metadata readers. Concurrent readers can observe an uninstalled holder before publication and refuse, or observe the complete installation after publication; no partially published pair is accepted. The creator's Callable reference retains the holder across registration/publication. A close beginning in that gap may make later export/registration fail, but cannot authorize a stale generation because those operations immediately revalidate live holder state. No new cleanup/owner copy or second function-pointer registry is introduced.

## Public memory boundaries

The helper validates non-null pointer, actual object alignment and non-wrapping half-open range before dereference. Query and register reject input/output overlap; relay additionally validates Callable object alignment/range and non-null receiver, and rejects output overlapping the input Callable. Size/alignment derive from the original C++/wire types. This does not claim to validate arbitrary invalid process pointers (the ordinary native ABI trust boundary); it correctly handles declared null/misaligned/overlapping/overflowed inputs before mutation. Output should remain untouched on these early refusals, which needs the planned negative tests.

## RT RELEASE budget and returned usage

The original service leaf `production_release_locked` resets its supplied usage output. Passing the same accumulated RT usage to it would erase maintenance costs; the patch instead creates a separate zeroed release_usage, subtracts already-used passes/receipts from the original capped immutable allowance, preserves the original deadline, and merges only verified usage afterward. It validates subtraction bounds before computing residuals, and validates returned counters before addition, so the sums stay within original capped limits.

Keeping controls unchanged in the residual budget is correct here: one original RELEASE dispatch has already been authorized, and controls serves as the existing leaf's authorization bit, not permission for a second queued dispatch. release_usage.controls must stay zero and is explicitly rejected otherwise; the original RT used.controls remains one. A residual zero passes/receipts or expired original deadline makes `service_budget_active` false for Scene content return. It cannot renew the deadline or consume more maintenance budget.

The content-return branch is after the original graph/native/physical-return/readback guards and before the native_issued readiness early return. Readiness-only preflight with no budget/usage still makes no source call. Actual RT RELEASE now supplies the residual budget, allowing one charged source return; owner reclaim still refuses remaining content and does not call the source. The temporary content Ref keeps its destructor out of Registry lock and capture_inflight remains held through the unlocked final drop. Original native-reference release and owner usage consumption remain the existing path; this patch does not mark native completion or change original failure cause.

## Waiting RECORD close without draw

The patch sets scene_waiting only when an actual queued RECORD is deferred for the real scene boundary, and clears it before normal FIFO execution. Unregister filters the same original RECORD kind, seals new phase delivery, and schedules at most one waiting original control per turn via the existing FIFO. It returns Busy before cached holder closure while such work remains. Subsequent turns continue the scan without requiring a draw. The previously reviewed multi-mailbox issue stays fixed: cached relay acknowledgement cannot skip the remaining waiting controls. OBSERVE/RELEASE controls are no longer accidentally labeled or rescheduled as phase waits.

Original owner report consumption remains separate from RT RETURNED; the registration close is not a substitute for consuming/releasing each original request. Active callbacks still prevent holder release through the original active_deliveries gate, and cached holder acknowledgement still derives from actual release/observe/forget.

## Remaining verification / limits

Run public alias/overlap/alignment/overflow negative cases with unchanged output and zero receiver/native effects; ordinary-normal-relay export/register refusal; exact typed holder success; wrong owner/generation and closed holder refusal. Exercise exhausted residual passes/receipts, partial maintenance plus one source-return cost, retained native observers, no-native retained content, and repeated Busy returns without double spending. Run two or more waiting RECORDs through unregister+FIFO without draw, and active callback/last external Callable reference closure.

The source review does not prove these runtime cases. OriginalOperationKey binding remains unavailable pending the actual B/Runtime exporter; this fix does not open Scene sample/read authorization or qualify Height/GPU rendering.
