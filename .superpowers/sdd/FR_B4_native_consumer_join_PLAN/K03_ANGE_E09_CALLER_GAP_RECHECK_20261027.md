# ANGE E09 production caller gap recheck — 2026-10-27

A fresh read-only census at ANGE `bd51f0c002` corrects the earlier caller summary. The internal `ProgramPreparationStore::acquire_native()` path is used by `qualification_scope.cpp:383`; that is an implementation-internal path, not a public loader producer.

The production tree still has no caller for the public `ParticleProgramLoader::request_native`, `poll_native`, `acquire_native`, or `request_qualification` entrypoints, and no caller for `StageExecutor::start_native_qualification()`. `bind_main_device()` has one real registration callsite at `src/register_types.cpp:335`. No lifecycle symbols were found, and Godot `MeshStorage::mesh_add_external_surface()` remains declaration/definition only.

The first missing production boundary is the original particle producer/admission path that must supply the public loader request and same-request E09 qualification start. No test helper, internal acquire, serial, or diagnostic is promoted to operation/completion/readback/Ready/GPU authority.

Evidence: `K03_ANGE_E09_CALLER_GAP_RECHECK_20261027_REPORT.json` and `/tmp/k03-current-native-owner-census-20261027.log`.
