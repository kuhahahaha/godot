# Queue close and resource release seam audit — 2026-10-24

Audited the native transport queue, capture writer close path, native-session retirement, and RD-pool release/error propagation.

No safe standalone production fix was found:

- Transport queue draining after poison is intentional: owner-thread FIFO drain remains available while further writes/pops from foreign owners are rejected and poison the queue.
- Capture writer byte accounting is a cumulative transport budget, not queue occupancy; changing it would alter the bounded-output contract.
- Native-session terminal registry removal is render-thread-owned and occurs only in retirement service.
- RD-pool release records retirement before freeing resources and preserves collector-drain and allocation-identity gates; changing ordering would cross the dirty renderer migration seam.

Verification boundary:

- Source audit: complete.
- No source mutation made in this slice.
- Full build not rerun; prior blocker remains `No space left on device`.
- Runtime/native/GPU/PCK/production qualification: false.
