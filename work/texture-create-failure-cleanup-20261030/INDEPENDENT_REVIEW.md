# Independent review — texture initial-data failure cleanup

Read-only review covered `_texture_create_internal()`, `_texture_initialize()`, qualification texture reservation, frame texture retirement, and the current Metal pair.

1. Non-BUSY initial-data failures now retire the failed texture through the normal frame path after marking any transfer-worker operation used by the draw.
2. ERR_BUSY unresolved completion deliberately keeps ownership; the existing K03 test asserts this and remains green.
3. Qualified failed creates clear the unpublished cell association before cleanup and avoid double-freeing an RID already retired by the helper. Reservation release remains caller-owned.
4. Existing owner/worker/fence semantics are preserved; no synthetic physical return or fallback was added.
5. Current pair still stops at E05 before texture/native admission. This package does not claim GPU success.

Residual coverage: a dedicated fake backend test for a non-BUSY failure after a partially initialized multi-layer texture would strengthen direct free-count evidence; source cleanup and existing unresolved-completion regression are covered here.
