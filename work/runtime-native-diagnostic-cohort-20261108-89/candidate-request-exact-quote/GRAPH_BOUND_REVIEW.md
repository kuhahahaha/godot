# Independent graph-membership bound review

Reviewer: independent Sol agent, 2026-11-08. Scope: frozen row graph math, actual RD staging/source capture, ProductionGraphCell/native membership mapping, worker/prefix behavior. Wrapper ABI is intentionally excluded. Reviewed candidate query/helper/README/tests and actual live source at HEAD80a75b2815010fa712f0d96ab87a6f9f6c3eab97. No live edits, builds, or tests performed.

## Verdict and blockers

**No blocking graph-bound arithmetic/source defect found for the declared supported frozen single-span action shapes.** `ceil(bytes/32)` bounds ProductionGraphCell membership for positive upload/readback bytes <=UINT32_MAX while staging block_size>=32. Dispatch/copy at most one graph membership each is consistent with actual capture. The native count is a conservative graph-edge membership quote using the existing live driver profile; it is not an unconditional bound against later profile/prefix drift, nor a native issue-count estimate. Original reserve/append checks remain necessary, as the candidate already states.

This is source/math approval of this narrow bound, not runtime acceptance. Actual fragmented/resumed/shared-prefix tests remain unexecuted and are a material acceptance gap. No same-graph producer/consumer join, source authority, ABI availability, or GPU conclusion is made.

## Chunk proof, including partially filled/shared blocks

`RenderingDevice::_staging_buffer_allocate` (rendering_device.cpp:979–1080) initializes alloc_size to requested amount. Both source routes request `min(remaining, block_size)` with required alignment32 and default can_segment=true. For any successful positive chunk:

- In a fresh/reused/inserted block, alloc_size remains min(remaining,block_size). If it is nonfinal, remaining>block_size>=32, so the chunk is at least32.
- In an already-used block, the write offset is rounded UP to32 alignment. A segment is allowed only when available>=32, and its size is rounded DOWN to a multiple of32. Therefore a segmented nonfinal successful chunk is at least32. Residual space below32 causes block selection/required-action handling, not a tiny emitted region.
- The full-fit branch may emit fewer than32 bytes only when requested amount itself is below32; because block_size>=32, this means remaining<32 and the chunk is final.
- Exact-fit handling uses the segmentation branch when appropriate; rounding cannot manufacture extra positive chunks below32. block_size need not itself be a multiple of32 for this inequality.

For B>0 bytes and k emitted positive chunks, all but at most the final chunk have size>=32, and the final is>=1. Thus `B >= 32*(k-1)+1`, so `k <= floor((B-1)/32)+1 = ceil(B/32)`. The helper computes the equivalent division+remainder form without overflowing B+31. B<=UINT32_MAX implies result<=134217728 and fits uint32. Zero B, unrepresentable B, and blocks below32 are explicitly refused.

The allocator may align away unused padding, select other blocks, insert a block, or request a stall/flush. Padding is not an emitted copy. In these captured Core paths, a required action returns BUSY before legacy blocking execution and publishes only previously accumulated positive regions. None of these branches splits a positive region into smaller-than32 nonfinal regions.

## Chunk count versus ProductionGraphCell count

Uploads: `_buffer_update` (:1207–1370) appends each chunk to a thread-local RecordedBufferCopy vector. `publish()` does nothing for an empty vector; otherwise `draw_graph.add_buffer_update` emits ONE record handle for the entire nonempty vector and advances source.consumed to the last region end. The graph implementation (rendering_device_graph.cpp:2246+) confirms one RecordedBufferUpdateCommand/handle even when it contains multiple copy regions. Therefore upload graph handles <=positive chunks, not one per driver copy call and not one per byte span in the vector.

Readbacks: `_buffer_get_data_async` (:1642–1805) stages regions, then publish() emits `add_buffer_get_data` and one capture segment per positive region. The graph implementation (:2227+) confirms one handle per region. This is the tighter count: readback graph handles equal emitted regions, hence <=ceil(B/32).

`production_record_effect_locked` (gdextension_production_action.cpp:490+) captures each source segment and only falls back to observed.handle when there were NO source segments. It never adds both source segments and an extra summary membership. `production_capture_graph_locked` (:261+) adds one ProductionGraphCell per distinct handle/action identity. Synchronization merely sets graph synchronization-pending state (rendering_device_graph.cpp:2967), so it creates no extra membership. Transfer staging/worker joins create native facts; they do not independently increment graph_count.

Uploads may return KnownPrefix and resume the same action. action.recorded_bytes/source.consumed are monotonic; later calls start at the untouched suffix. Successful nonfinal prefixes contain only nonfinal chunks>=32, and already-published chunks are not replayed. Thus the proof applies across all attempts, not merely one call. Readback Core publication also advances state.issued; the current production route cancels a non-upload KnownPrefix instead of granting its continuation. Even counting all regions before cancellation remains within the same bound. Arena/capture capping returns before consuming another readback region; publication cannot add an empty handle.

## Frozen source shapes and summation

The candidate correlates exactly one UploadV2 row per upload action and rejects duplicates/orphans. One row is one contiguous destination span/RD call; its internal staging vector is already covered above. Multiple actions must sum THEIR individual ceilings, which the candidate does; it does not ceil the aggregate byte sum. Action uses can contain several logical storage spans, but they do not cause multiple upload/readback calls inside this mechanical path.

Each readback action claims exactly one typed readback row/slot, checked both directions. REQUEST_WRITER and RETAINED_INCOMING source forms ultimately call the same `_buffer_get_data_async` on one actual storage resource, offset, and byte_size. The source-kind checks and incoming ownership/join remain original reserve/freeze responsibilities. Those forms do not introduce extra texture regions, scatter/gather spans, or per-producer graph records. Non-storage/alias/native-output shapes are refused by this query. A future vector/scatter readback shape would require sum-of-per-span bounds; this formula must not be generalized to it without new source proof.

Every supported positive action has bound>=1. Let total G be the sum of action bounds. The record path keeps one credit for each later action and offers the current action `G - graph_count - later_action_count`. Earlier completed actions consumed no more than their own bounds; current continuations consumed no more than their globally bounded chunk total. Therefore this one-credit reservation policy cannot underallocate the current action while preserving the later bounds: additional later-action credits remain extra capacity. The query uses checked uint32 summation and final capacity_fits; it refuses an excessive bound rather than clipping it to the pool. Rejecting zero-byte transfers is conservative because the raw legacy readback route can interpret size0 as full buffer; the typed production declaration already requires positive byte_size.

Dispatch: `_record_source_dispatch` returns one dispatch handle; list-close fallback is used only if observed.handle is absent. Copy: `_record_source_buffer_copy` carries one actual.copy_handle from one `_buffer_copy_recorded`. Failure can expose a prefix/unknown obligation but does not create a second captured graph membership in these branches. Group/list bindings are not separately inserted ProductionGraphCells.

## Native mapping / shared worker prefix

`production_capture_issue_locked` creates native rows per (graph membership parent, original source identity), retaining every alias edge even if several graph records share one command slot/source. Deduplication is per parent; globally unique source count is NOT the arena requirement. Candidate `graphs * per_edge` uses the correct membership interpretation.

The reused `production_membership_bound_locked` asks the ORIGINAL driver profile for one main slot, bounded worker slots and one frame, then explicitly adds the post-graph PREPARE_SEGMENT bound. Metal3's actual profile (drivers/metal/rendering_device_driver_metal3.cpp:266+) includes up to3 main calls, up to2 calls per worker, one pre-graph hidden copy source, and all currently pending completion records; the added prepare source covers the second prepare phase. One actual worker submission passes one command buffer, no waits, and optional signals; a shared worker batch may contain many operations but they do not become one source per operation. Existing wrapper prefix records are counted by the original profile, not discarded by assuming only newly emitted queue calls.

Distinct declared STORAGE slots per action are a safe worker upper bound for the supported frozen closure. Original dispatch closure requires every actual storage binding to be covered by an action use, and rejects unsupported shared-texture/pending-clear profiles. Copy/upload/readback likewise validate their actual storage uses. Several slots may share one worker, making this conservative. Clamping to the original RD pool maximum is valid because the actual pool cannot use more workers than that maximum. Worker transfer facts are correlated to exact operation ranges/action ordinals, not all driver work merely sharing the queue.

**Temporal limit:** query profile is current at the query, not a reservation of future pending wrapper state. Later work/profile drift may make reserve/append reject the quote; actual append recomputes required native credits before effects. This candidate retains that original behavior and documents it. A claim that the returned native_memberships guarantees arbitrary later concurrent source expansion would be false; this review does not certify such a claim.

## Required focused acceptance beyond arithmetic unit tests

Before claiming operational no-underquote acceptance, exercise actual source capture on a partially filled block with exactly32 residual bytes and a final1-byte tail; upload aggregation of multiple regions into one graph; a resumed KnownPrefix across staging exhaustion; readback one-handle-per-region across several blocks; multiple small transfer actions where sum(ceil) differs from ceil(sum); both readback source kinds; and a nonzero worker/shared pending-wrapper prefix followed by prefix drift rejection. Assert ProductionGraphCell count and per-parent native membership, not driver command count alone. These are acceptance gaps, not a discovered arithmetic counterexample.
