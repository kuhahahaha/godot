# Production raster staged implementation

Source implementation and focused tests: `b75484dcf1` (combined core/test commit). This is an incomplete step of the full Production raster and Sprite LE192 goal.

The accepted arena now contains one independent raster action and four unprepared request-owned physical resource slots. Freeze and RECORD scheduling refuse an unprepared reservation, preventing an empty successful RECORD. Typed begin/end validate the original physical callback, request, control, thread, exact owned physical cells and remaining graph capacity. Draw closure participates in callback return and pending obligations. No qualification raster import exists.

Follow-up independent framebuffer/depth/shader/pipeline creation and prefix-failure retirement are now implemented and CPU-verified; see RESOURCE_PREPARATION_CONTRACT.md and RESOURCE_PREPARATION_RECEIPT.md. Successful checked begin/end and graph execution through the original service path remain the next integration boundary. No visible draw or GPU completion is claimed. Sprite LE192 writer/receiver and visible draw acceptance remain outstanding.

Verification on 2026-11-06:

- ABI JSON generation succeeded.
- Using compile_commands.json entries with output compilation replaced by `-fsyntax-only -O0`: production_action, production_interface, production_owned, production_request, production_window and test_production_request all exited 0. This checks syntax, not linking or runtime behavior.
- Previous binary (before the new raster implementation): ProductionRequest 55/55 cases and 36,484 assertions; particles 18/18 and 90 assertions. This is baseline evidence only, not verification of b75484dcf1.
- Independent read-only review found no blocking issue within the explicitly incomplete scaffold; actual resource preparation and native authorization remain unimplemented.

A current tests=yes, debug_symbols=no editor rebuild is tracked separately; runtime focused tests must be run on that newly linked binary before claiming the new behavior passes.

Follow-up verification on 2026-11-06: the full editor build with tests=yes, debug_symbols=no and module_particles_runtime_tests_enabled=yes completed successfully. The newly linked binary passed raster 2/2 cases (146 assertions), all ProductionRequest 60/60 (38,304 assertions) and particle CPU tests 18/18 (90 assertions). The raster arena-capacity error expectation was corrected to ERR_INVALID_PARAMETER. See RECORD_HISTORY_RECEIPT.md and its JSON manifest for commands, identities, logs and limits. Those results verify the scaffold guards. Subsequent resource-preparation results are documented separately; positive native draw remains outstanding.
