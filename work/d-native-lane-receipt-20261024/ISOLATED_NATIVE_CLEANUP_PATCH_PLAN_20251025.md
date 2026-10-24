# Isolated Native Cleanup Patch Plan

Date: 2026-10-25

No safe standalone source patch was found in this pass. The remaining concrete
native cleanup findings are all in the mixed-dirty
`servers/rendering/rendering_device.cpp` migration file:

1. `buffer_get_data()` map failure: free the temporary readback buffer before
   returning.
2. `texture_get_data()` map failure: free the temporary readback buffer before
   returning.
3. `_buffer_update()` dynamic-persistent map failure: validate the mapped
   pointer before `memcpy` and return a checked error.
4. `_texture_create_reinterpret_buffer()` buffer creation failure: fail closed
   before memory accounting/tracker publication.
5. `texture_create_shared*()` fallback creation failure: roll back fallback
   texture/tracker and `texture_memory` before returning.
6. `tlas_build()` validation failure: stage all instance/dependency mutations
   and commit only after complete validation.

Implementation decomposition for the owning renderer migration:

- Commit A: add narrow cleanup guards for synchronous buffer/texture map
  failures, with test-driver counters asserting temporary `buffer_free` calls.
- Commit B: add dynamic-persistent map failure handling with an injectable
  null-map driver test and no write-through assertion.
- Commit C: add rollback helpers for reinterpret/shared fallback allocations,
  covering driver-create failure and tracker/accounting invariants.
- Commit D: implement two-phase TLAS build validation/commit and focused tests
  for invalid hit SBT, wrong BLAS type, and invalidated BLAS preserving prior
  state.

Verification commands once the owner-attributed migration is isolated:

- `scons platform=macos target=editor tests=yes module_skinned_mesh_capture_tests=yes`
- focused rendering transfer/acceleration tests;
- `git diff --check`;
- independent source review of each commit.

Current evidence is source/static only. The full build is blocked by
`No space left on device`; runtime/native/GPU/PCK/platform/performance/
production qualification remains false.
