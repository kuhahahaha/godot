# Second native error/cleanup slice blocker — 2026-10-24

A second candidate slice was audited after the `mark_enqueued` blocker:

- `record_public()` and `record_native()` retain partially created resources in the slot after a failure; this is intentional because the session closes through `RDPool::release()`, which owns the complete slot/resource retirement path. Adding local frees would bypass the retirement ledger and alter cleanup ordering.
- `record_image()` already has explicit failure rollback in `137833abc2`; no equivalent unowned image path remains.
- `enqueue_readback()` cannot safely synthesize a rollback after successful RD submission and failed `mark_enqueued()`, because the callback may already be retained by RD.

Disposition: no additional standalone source patch. The next safe implementation requires an explicit retained-but-unmarked collector state and corresponding session/retirement contract, plus focused runtime evidence.

Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
