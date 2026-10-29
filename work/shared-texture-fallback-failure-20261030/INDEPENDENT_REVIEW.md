# Independent review — shared texture fallback failure ordering

Read-only review covered `texture_create_shared()`, `texture_create_shared_from_slice()`, `_texture_free_shared_fallback()`, sampling uniform setup, and the current editor/DSO pair.

1. The regular shared-view driver result is now checked before any fallback allocation.
2. The independent fallback texture is now created into a temporary handle and checked before publishing `SharedFallback` or calling allocation-size accounting.
3. Success paths retain the existing tracker, revision, owner/dependency, and sampling behavior; failed paths return a null RID without adding a dependency or fallback object.
4. The pair still refuses at E05 owner transport before texture sampling/native work. This is an upstream owner input and remains fail-closed.
5. No unrelated worktree paths were staged.

Residual audit item: `_texture_create_reinterpret_buffer()` still has a void API and assumes backend allocation succeeds. Propagating that failure requires a broader caller/cleanup contract, so it is recorded rather than masked by this narrow patch.
