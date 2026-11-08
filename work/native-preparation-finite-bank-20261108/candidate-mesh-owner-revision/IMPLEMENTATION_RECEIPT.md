# Original Mesh revision implementation

Mesh owns a mutation-driven state revision, captured in each actual native selection lease and exposed only while that original lease is active. Existing selection guards freeze source changes; exhaustion never wraps to an older token. Original full RID remains the allocation/lifetime identity. This is state revision, including region metadata invalidation before potentially partial RD writes, not GPU completion.

Independent review caught a zero-count rejected add changing bone metadata and missing stale-null shadow notifications. Both fixed and re-reviewed: reject before Surface allocation, publish bone state with actual directory, preserve all old notifications while advancing only actual shadow clear. Tests exercise rejected metadata/allocation prefix, real dependency callbacks, independent held revision, mutation/refusal/reacquire, MAX selection return and permanent exhaustion.

Exact restored source built and passed2cases/437assertions, combined83/21701, process0. Omitting only the initial path revision advance produced4 target failures/process1 with normal cleanup; exact source restored/rebuilt and rerun. Actual Metal Mobile/Forward+ camera rendering neighbors passed. RUNTIME_RECEIPT.json pins source/host/commands and remaining limits. Public mesh-facts transport and complete qualification remain active work.
