# Preparation bank metadata layout receipt

Task 2 first slice only: metadata preparation count is independent of surface binding count. Generation planner explicitly quotes global read_lease_capacity preparation rows. Legacy internal planner keeps its explicit default binding count. Placement, source revalidation and Acquire slab validation all use the same preparation_capacity. All constructed rows are returned; unbound rows must be completely closed.

RED: GenerationSelectionMultiMesh 2 cases / 430 assertions, 4 failures: old layout 25368 bytes versus independently computed 24464. The expectation sums actual type sizes/alignment with read-limit preparation count independently from planner fields.

GREEN build: `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`, exit 0. Binary SHA256 034daca6e7e116a71456a82fe750677ceb5dc795cebd0021ae2efad42786278e.

GREEN filter PreparationBankLayout/GenerationSelection/GenerationSurfaceSource/NativePreparationContinuation/GenerationMetadataLayout/PipelinePrefix/AliasReadbackIntegration: 17 cases / 6987 assertions, all pass, exit 0. Includes explicit preparation-only/zero bank, maximum acceptable extent/first overflow and unchanged output on refusal. Local logs PREPARATION_BANK_RED.log and PREPARATION_BANK_GREEN.log retained.

Independent scoped Sol review clear; diff/format checks clean. Default Sol only, no Astra. CPU mechanics only; no GPU/visible draw or public caller claim.

Still open: actual atomic P-row allocation, stable tuple/entry identity, pre-reserved complete geometry/material/pipeline groups, CloseOldWork capture/adoption, bank retirement, public query/Create/Seal/read/renderer integration and original U05/native association. Existing slot*surface locator remains a guarded compatibility path; this layout slice is not a complete tuple bank.
