# Mixed-Dirty Owner Isolation Check

Date: 2026-10-25

The requested minimum map-failure cleanup patch was checked against the live
working tree. `servers/rendering/rendering_device.cpp` currently has a large
mixed migration diff (`482` additions and `174` deletions) spanning the same
functions and surrounding include/observer changes.

Concrete hunk locations:

- synchronous buffer readback map failure: lines 1365-1366;
- shared texture fallback validation/creation: lines 1894-1930;
- texture upload staging path: lines 2415-2494;
- reinterpret buffer creation: lines 2670-2680;
- texture readback map failure is in the later `texture_get_data()` block;
- dynamic-persistent map failure is in `_buffer_update()` near lines 1100-1110.

Because these hunks share the same migration regions, an isolated commit would
claim unrelated renderer ownership and cannot be safely prepared in this
worktree. No source mutation was made.

## Required commit split when ownership is available

1. Map-failure cleanup: buffer and texture synchronous readback, with driver
   free-count tests.
2. Dynamic-persistent map guard, with null-map driver test.
3. Reinterpret/shared fallback rollback, with accounting/tracker assertions.
4. TLAS transactional validation/commit, with invalid-input state-preservation
   tests.

For each split: RED narrow test, minimal hunk, GREEN narrow test, independent
review, `git diff --check`, then named commit. Full build/runtime/GPU evidence
remains blocked by `No space left on device`; qualification flags remain false.
