# P-bank reservation fix review

## Scope

Reviewed the minimal `return_preparation_row` preflight in `servers/rendering/renderer_rd/storage_rd/external_output_owner.cpp` and the `preparation_bank_fragmentation` regression coverage. This is a read-only review; no source or test edits were made here.

## Finding

The restored preflight is correctly placed before delegation to `return_preparation_bank_row`:

1. It validates the slot and surface bounds.
2. It preserves the legacy unbound case: a null row is accepted only when both the preparation pointer and locator are clear.
3. For a bound surface, it calls `preparation_row_matches` using the caller's actual `(slot, surface)` and the surface's current row pointer. This binds the pointer, locator, owner, owner surface, generation, lease serial, and (when complete) source tuple together before any row is released.
4. Only after that validation does it delegate using the surface's locator.

This closes the previously unsafe sequence where a caller-corrupted surface locator could cause delegation to release a different live bank row. In the fragmented case, surface 0 can point at row 0 while its locator is changed to row 4; the preflight rejects the mismatch and leaves both rows and the surface association intact. Swapping both pointers and indices between surfaces is likewise rejected because `owner_surface` is part of the match.

`return_preparation_bank_row` still handles rows without a surface (`owner_surface == UINT32_MAX`) through the bank-row predicate alone. That preserves retirement of extra rows allocated by `bind_preparation_rows(slot, count)` while retaining owner slot, generation, lease serial, sealed-source, and resource-state checks. Surface-specific cleanup is skipped only for those intentionally unassociated rows.

## Evidence reviewed

- Diagnostic RED log: `/tmp/b4-p-reservation-bank-diagnostic-test.log` — 401 assertions, 399 passed, failures at test lines 434 and 453.
- Fixed GREEN log: `/tmp/b4-p-reservation-final-test.log` — 401 assertions, 401 passed; one test passed.
- Regression exercise covers fragmented reuse, wrong pointer/index, swapped surface associations, lease serial corruption, interleaved returns, retire-cursor reset, and final row cleanup.

The logs are supplied runtime evidence; this review did not rerun the test or perform a build.
