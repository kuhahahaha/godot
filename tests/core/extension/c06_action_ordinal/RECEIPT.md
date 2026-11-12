# C06 physical action ordinal V2

V2 separates a physical action index from the original logical canonical range. Multiple upload, dispatch and readback actions can share a canonical value; an empty physical slice uses no dummy action. Frozen rows retain the source recipe substep. Preparation, graph/native facts, transfer membership, readback endpoints and segment/report frontiers use the physical index. Original source mapping and publication authority remain with their owners.

The JSON interface appends ten DTOs and eleven entry points. Existing V1 rows and their order remain identical. Explicit V1 adapters preserve original error priority, output clearing and budget behavior. Authenticated version mismatches reject before changing caller output, payload or service control. Shared lifecycle, resource and ACK paths retain the original owner.

## Verification

The exact fourteen source hashes and host identity are recorded in RECEIPT.json. The macOS editor with tests and particles_runtime_tests enabled built successfully. Targeted tests passed 50 cases and 33,413 assertions; the full original-bin suite passed 2,089 cases and 523,669 assertions, with zero test failures. Full-suite diagnostic output is preserved in the local evidence directory; it is not claimed empty.

The new component cases cover same-canonical ordinal correlation, genuine zero capacity through the installed public FIFO, V1 compatibility, readback producer/range validation, native membership and deterministic completion, and KnownPrefix cancellation/ACK/physical return. Native component cases use the existing CPU backing driver and do not execute shaders on hardware. KnownPrefix does not establish Unknown handling or successful V2 suffix retry.

A separate run on the same pinned host passed the existing actual main-Metal particle fixture: 390 checks, eight asynchronous readbacks, 31 allocation observations and 29 native resource returns. This verifies that fixture only. It does not prove V2 GPU shader qualification, original source admission or whole output release.

Final clang-format 21 checks, codespell, C11/C++17 generated-header layout/POD checks and original V1 JSON preservation checks passed. Independent source, V1 compatibility, runtime-fix and native-test reviews are pinned in the machine receipt; no remaining static blocker was identified in their specified scopes.

## Reproduction and evidence

Build with `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3`. Run the editor from the original bin path with `--test`, setting `GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D` and `GODOT_TEST_PHYSICS_3D_THREADED=0`. The pinned native fixture command and strict result are in `work/b4-c06-v2-20261106/native390/RESULT.json`.

Local evidence under `work/b4-c06-v2-20261106/` preserves initial failed runs, incremental fixes, exact build commands/source pins, XML, stdout/stderr, immutable host and controlled metadata. These large artifacts are not added to Git. This component does not close the continuing native/Sprite/output queue.
