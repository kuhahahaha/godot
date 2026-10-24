# Acceleration Deferred Memory Audit

Date: 2026-10-25

Audited deferred acceleration-structure destruction. The disposal path frees the
native acceleration structure first, then subtracts and frees its scratch
buffer, unmaps/frees every instance buffer, and updates `buffer_memory` under
the thread-safe lock. Owner removal queues the complete acceleration structure
state before releasing the RID, so deferred cleanup retains all native handles.

No safe independent production patch was identified. Runtime/GPU receipts are
still unavailable and the full build remains blocked by disk space; all
qualification flags remain false.
