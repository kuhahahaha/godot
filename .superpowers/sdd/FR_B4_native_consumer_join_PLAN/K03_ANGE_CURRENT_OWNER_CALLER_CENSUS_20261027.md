# ANGE/Godot current owner-caller census — 2026-10-27

A source census after the current ANGE host/capture commits (`fd555ff`) confirms the new bounded host service and internal preparation work without overstating its reach.

- Public `ParticleProgramLoader::request_native`, `poll_native`, and `acquire_native` have no external production callers. The internal `ProgramPreparationStore::acquire_native()` path is used at `src/particle/runtime/qualification_scope.cpp:383`.
- Public `ParticleProgramLoader::request_qualification` and `StageExecutor::start_native_qualification()` have no production callers.
- `bind_main_device()` has one real registration callsite at `src/register_types.cpp:335`.
- Godot `MeshStorage::mesh_add_external_surface()` remains declaration/definition only.

The new host/capture commits therefore improve bounded owner/session/storage behavior but do not close the original producer/admission, same-request E09 association, operation, completion, or readback boundary. No synthetic caller, internal acquire, serial, or test path is promoted to authority.

Evidence: `K03_ANGE_CURRENT_OWNER_CALLER_CENSUS_20261027_REPORT.json` and `/tmp/k03-current-source-owner-census-fd555-20261027.log`.
