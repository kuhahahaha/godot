# Production raster staged implementation

Source implementation and focused tests: `b75484dcf1` (combined core/test commit). This is an incomplete step of the full Production raster and Sprite LE192 goal.

The accepted arena now contains one independent raster action and four unprepared request-owned physical resource slots. Freeze and RECORD scheduling refuse an unprepared reservation, preventing an empty successful RECORD. Typed begin/end validate the original physical callback, request, control, thread, exact owned physical cells and remaining graph capacity. Draw closure participates in callback return and pending obligations. No qualification raster import exists.

Independent framebuffer/depth/shader/pipeline creation and failure retirement are still absent. Consequently positive native begin is unreachable via the public preparation path; no successful draw, GPU completion or physical retirement is claimed. Sprite LE192 writer/receiver and visible draw acceptance remain outstanding.

Verification on 2026-11-06:

- ABI JSON generation succeeded.
- Using compile_commands.json entries with output compilation replaced by `-fsyntax-only -O0`: production_action, production_interface, production_owned, production_request, production_window and test_production_request all exited 0. This checks syntax, not linking or runtime behavior.
- Previous binary (before the new raster implementation): ProductionRequest 55/55 cases and 36,484 assertions; particles 18/18 and 90 assertions. This is baseline evidence only, not verification of b75484dcf1.
- Independent read-only review found no blocking issue within the explicitly incomplete scaffold; actual resource preparation and native authorization remain unimplemented.

A current tests=yes, debug_symbols=no editor rebuild is tracked separately; runtime focused tests must be run on that newly linked binary before claiming the new behavior passes.
