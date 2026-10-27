# K03 acceptance matrix v41 — 2026-10-28

This successor matrix preserves v40 and adds the green opt-in C04 SPIR-V destruction target. The fix is confined to test-support allocation headers; production SPIR-V reflection/destruction behavior is unchanged.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| ANGE C04 SPIR-V destruction target | `70fa12ff0` | `test-spirv-destroy` builds and runs rc0, 1 case / 224 assertions; fixture allocation now matches production `ReflectFree` header layout | opt-in target only; no default coverage, native/GPU, owner, completion/readback, or Ready claim |

The v40 fixed editor/DSO pair and all authority claims remain unchanged.
