# Independent review — readback temporary-buffer release

Read-only review of the two synchronous readback paths, their wait/map/free ordering, existing asynchronous map-failure tests, and the rebuilt current editor/DSO pair.

1. `buffer_get_data()` previously leaked `tmp_buffer` only after a null `buffer_map()` result. The repair frees exactly that temporary allocation and returns an empty vector.
2. `texture_get_data()` had the same leak after its texture-to-buffer transfer and blocking stall. The repair mirrors the buffer path.
3. Normal success paths still unmap and free exactly once; the wait-error path still defers disposal through the frame queue. No double free path was introduced.
4. Existing K03/R01 map/readback controls remain green. The current Metal pair reaches E05 before resource admission, so it cannot exercise a qualified native readback; this is recorded as an upstream owner limitation.
5. No unrelated dirty/deleted/untracked files were staged.

Residual coverage: a future test-only fake driver can return a non-null temporary `BufferID` and null `buffer_map()` to count the new synchronous free directly. The current suite exercises the same map-null contract through asynchronous/frame-wait paths and all focused suites pass.
