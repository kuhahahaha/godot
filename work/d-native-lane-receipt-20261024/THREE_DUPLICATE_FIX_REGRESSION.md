# Three duplicate lifecycle fixes regression — 2026-10-24

Static regression confirms all three poison transitions and their focused assertions remain present:

- image `reject()` after an already rejected slot;
- readback `mark_enqueued()` after an already enqueued identity;
- readback `cancel_unqueued()` after an already canceled identity.

Each transition preserves holder accounting and does not decrement `pending` twice. The callback destructor remains the sole holder-release path. No additional holder/pending resource-release patch is justified: the remaining candidates are either impossible double-destruction states or depend on renderer/owner integration semantics.

Qualification remains bounded source/static only. Existing binary lacks the optional module selectors; full build remains blocked by `No space left on device`.
