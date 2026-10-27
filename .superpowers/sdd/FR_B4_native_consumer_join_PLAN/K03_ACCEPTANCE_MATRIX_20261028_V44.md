# K03 acceptance matrix v44 — 2026-10-28

This successor matrix preserves v43 and adds a current rerun of the opt-in C04 SPIR-V target after the allocator fixture alignment and production reflection ordering fix.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| ANGE current C04 SPIR-V rerun | `77a9385aa` | Opt-in `ange_test_c04_spirv`: 66 cases / 6985 assertions, rc0, 0 failures | default graph remains opt-in; no native/GPU/owner/operation/completion/readback/Ready/Verified claim |

The v43 fixed editor/DSO pair and all authority claims remain unchanged.
