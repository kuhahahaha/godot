# Real main-RD guard mechanism fixture

Status: Vulkan finite mechanism checks pass; native Metal is blocked at INIT pipeline compilation. This fixture changes no production source and starts no engine build.

Host: `/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64`, SHA256 `4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528`, source identity a2f009e6c. The host supplies existing public RenderingDevice APIs; the current guard shader is compiled separately and uploaded through RDShaderSPIRV. The host does **not** contain or execute the new C++ guard wrapper.

Shader SHA256: `4570c3136e27a3beb699593d1e496119fa677d44c3cc5abfe31c5e2712c982d7`. All five production variants and one targeted INIT mutant compile through `glslangValidator -V` and validate with `spirv-val`. Per-variant source/SPIR-V hashes, exact host command, process exits, durations and raw-log hashes are in backend result JSON. `initial-artifacts/` preserves actual shader/SPIR-V/input/script artifacts for these runs; `INITIAL_EVIDENCE_MANIFEST.json` records their identities.

## Vulkan execution

Command: `python3 work/b4-guard-main-rd-gpu-20261104/run.py --backend vulkan`.

Actual banner: Vulkan 1.2.357, Forward Mobile, Apple M3 Pro. Main RD is obtained and all operations run through RenderingServer.call_on_render_thread. Each case creates actual storage buffers, its shader-compatible uniform sets and compute operations: INIT, compute barrier, each bounded 64-record VALIDATE chunk with intervening barrier, then FINALIZE. Capacity-zero uses INIT_EMPTY/FINALIZE_EMPTY, omits record allocations/descriptors and dispatches no VALIDATE. No main-RD submit/sync is invoked.

The 12 named cases are healthy, count zero, capacity zero, invalid source, source/count mismatch, actual over capacity, one current geometry escape, history-reset ignoring invalid previous geometry, valid-history rejecting invalid previous geometry, history not permitted, two surfaces and two chunks (65 records). Inputs and expected status/commands are independent finite identity/bounds examples; expected arrays are not calculated from production shader execution. GPU result words are compared exactly, converting JSON numeric literals to integers explicitly.

- Baseline: exit 0, 12/12 exact status and command arrays pass.
- Targeted source-validity mutant: exit 1 as expected, one source_invalid row fails the same unchanged literal oracle. Actual status `[0,1,0,0,55,0,1,0]` and command `[6,1,0,0,0]` differ from the required source fault/zero command; there are no engine ERROR/fatal diagnostics. This is the specific expected failure, not a crash or generic failure.
- Restored production: exit 0, 12/12 exact arrays pass again.

Each row completes two actual buffer_get_data_async callbacks before its result is accepted. The 60-second timer only fails missing callbacks; neither elapsed frames nor RenderingServer CPU sync is accepted as GPU completion. These readbacks introduce observation/synchronization and do not prove a no-stall execution schedule. Cleanup calls public free_rid only after callbacks and records **free requested**, not actual physical retirement.

All three Vulkan logs preserve five bare `OpArrayLength is not supported yet.` messages. Their source is optional re-spirv optimizer parsing (`thirdparty/re-spirv/re-spirv.cpp:1861`); `drivers/vulkan/rendering_device_driver_vulkan.cpp:5180–5220` falls back to the original decoded SPIR-V if that parser returns an empty shader. The actual pipeline/readback results above are still finite evidence. This does not claim every optimizer or range-check configuration is qualified.

## Metal failure retained

Command: `python3 work/b4-guard-main-rd-gpu-20261104/run.py --backend metal`.

Actual banner: Metal 4.0, Forward Mobile, Apple M3 Pro (Apple9). Shader RID creation succeeds; INIT compute pipeline creation fails while compiling generated MSL. Exact diagnostic: reinterpret_cast from `const constant uint *const constant` to `device CurrentRecords *const __restrict constant &` casts away qualifiers, with the expression referencing `spvDescriptorSet0.spvBufferSizeConstants`. Exit 2, no guard dispatch/readback case accepted. The full failure is in `initial-native-compile-failure-metal-baseline.log` and its result JSON. The first failure path also leaked the one Shader RID on shutdown; the fixture fatal path was then fixed to request cleanup on the main render thread. The old warning/error is retained and is not reported as clean Metal execution. No rerun or production shader/backend fix is claimed here.

## Scope

This is real GPU **shader mechanism** execution with directly constructed fixture buffers. It is not E09 source authority, source bridge/association, native C++ wrapper preparation, renderer draw/adoption, material qualification, view/history tuple integration, no-readback scheduling, native physical-disposal completion or overall B4/ANGE qualification. The source validity words deliberately supplied by this fixture are data for branch testing and confer no producer capability. Public free is a cleanup request only.


## Native Metal container diagnostic export

One additional sequential Metal host invocation (`-- --export-metal`) exits 0 and exports the five original compiled shader containers plus the independent reviewer's validate-only-records INIT candidate. The actual command, host/script/log hashes and six output byte counts/hashes are in `metal-container-export.json`; stdout/stderr are in `metal-container-export.log`. Output containers are in sibling `work/b4-guard-metal-diagnosis-20261104/`. This calls shader_compile_binary_from_spirv only: it creates no compute pipeline, submits no guard operation and provides no additional GPU success evidence. The original native pipeline failure above remains unresolved by this export. The reviewer will decode MSL offline.
