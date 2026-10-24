# RDPool partial enqueue/retirement blocker — 2026-10-25

Audited RDPool resource-token registration, retirement token capacity, and `enqueue_pair()` partial success behavior.

The pool retains registered resource tokens until physical retirement acknowledgement. `track_resource()` fails closed on missing driver identity, token exhaustion, or registry exhaustion; callers either roll back newly created RIDs or enter the existing release path. If the first readback enqueue succeeds and the second fails, the first callback remains retained and `r_enqueued` records the accepted prefix, preserving holder/pending accounting for close.

No safe standalone patch is justified. Canceling the accepted first callback after a later pair failure would violate the retained-callback contract and could race the render-thread delivery.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
