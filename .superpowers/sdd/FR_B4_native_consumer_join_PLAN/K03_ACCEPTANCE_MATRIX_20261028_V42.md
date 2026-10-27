# K03 acceptance matrix v42 — 2026-10-28

This successor matrix preserves v41 and adds the current capture-reader source/contract test result. The bounded subset is green; the four full-suite environment errors are retained as fixture boundaries, not source failures.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| ANGE capture reader contract current | `77a9385aa` | Bounded unittest subset 118 tests / 0 failures / 0 errors; full discovery 119 tests with 4 missing-environment errors and 0 source failures | E34/P01 fixture environment, runtime owner/capture transaction, native/GPU, completion/readback, Ready/Verified remain open |

The v41 fixed editor/DSO pair and all authority claims remain unchanged.
