# Independent review — reinterpretation buffer failure propagation

Read-only review covered `_texture_create_reinterpret_buffer()`, both shared-texture callers, `_texture_free_shared_fallback()`, driver buffer accounting, and the current pair.

1. The helper now rejects null input/fallback and invalid buffer creation before calling allocation-size accounting.
2. Successful creation publishes the buffer and tracker only after the backend handle is valid; repeated calls are idempotent when a buffer already exists.
3. Shared alias failure cleans its fallback texture/tracker and regular view. If it created the source fallback solely for this attempt, that empty source state is also removed.
4. Slice failure cleans its fallback texture/tracker and regular view. Existing owner/dependency and sampling semantics are unchanged.
5. RenderingDevice/Qualification/K03/R01 focused tests pass. Metal controls remain E05 rc2 before texture/native work, so no GPU success is claimed.

No unrelated dirty/deleted/untracked paths were staged. A direct fake backend allocation-failure test for the raw reinterpretation branch remains future coverage; source cleanup and fail-closed propagation are explicit.
