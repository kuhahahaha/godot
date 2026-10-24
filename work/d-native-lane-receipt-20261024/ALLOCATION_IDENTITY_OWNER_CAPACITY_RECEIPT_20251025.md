# Allocation Identity Owner Capacity Reproduction Receipt

Date: 2026-10-25

## Ownership boundary

Both the helper and its only focused test include are untracked migration files:

- `modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h`
- `modules/skinned_mesh_capture_tests/tests/test_physical_registry.h`

`git ls-files` returns no tracked entry for either file. They cannot be safely
patched or committed without claiming the surrounding migration slice.

## Deterministic reproduction

At `skinned_mesh_capture_test_observer.h:237-251`:

1. Call `acquire_owner(7, 11, owner)` eight times with distinct owners.
2. Call `acquire_owner(7, 11, 9)` for a ninth owner.
3. `acquire()` increments the shared entry reference count before the owner
   slot scan; all eight slots are occupied, so the scan fails.
4. The function returns the existing serial instead of `0`, leaving one
   ownerless reference.
5. Releasing the eight owners still leaves `references == 1`; the subsequent
   `physical_dispose(7, 11)` returns false.

## Required regression assertions

```cpp
for (uint64_t owner = 1; owner <= 8; ++owner)
    CHECK(identity.acquire_owner(7, 11, owner) != 0);
CHECK(identity.acquire_owner(7, 11, 9) == 0);
for (uint64_t owner = 1; owner <= 8; ++owner)
    CHECK(identity.logical_free_owner(7, 11, owner));
CHECK(identity.physical_dispose(7, 11));
```

## Receiving order

1. Track the helper/test migration owner.
2. Refactor `acquire_owner()` to locate a free owner slot before incrementing
   references; return `0` when none exists.
3. Add the regression above to `test_physical_registry.h`.
4. Run the focused physical-registry test, then `git diff --check` and an
   independent source review.

No source mutation was made in this mixed, untracked migration tree.
Qualification remains source/static only; runtime/native/GPU/PCK/platform/
performance/production flags remain false.
