# Metal runtime-array length controls

Status: finite controls prepared and executed against the frozen **old** host. Vulkan aligned controls pass; old Metal no-length control passes and actual `.length()` shader detects the specific auxiliary-binding collision. **No repaired Metal host has been run.** No production source or old guard evidence was modified, and no engine build was started.

All invocations use `/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64`, SHA256 `4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528` (a2f009e6c). Each invocation creates a new timestamp/UUID directory containing immutable source/script/input/SPIR-V snapshots, complete command/output, host and file hashes. Existing runs are never overwritten. A frozen-path hash mismatch refuses the run.

`run.py` accepts `--godot /absolute/new/host` for future fix verification; that option is implemented but **has not been used**. The default remains the frozen old host. No fixed-host GREEN is inferred from the controls below.

## What is exercised

Both shaders use truly active storage buffers at set0 binding0, set0 binding3, set0 binding5 and set1 binding2. The first buffer contains `uint[]`, binding3 contains an exact 16-byte uvec4 prefix followed by `uint[]`, set1 contains `vec4[]`, and the output contains four u32 words. All storage blocks are read/written: the output receives four lengths; input last elements receive values calculated from first elements and the prefixed header. Readback compares every word of all four buffers, including unchanged prefixes and exact float bit patterns. This checks array stride/address offsets together with lengths rather than accepting a constant output from an inactive binding.

Shader A uses finite literal indices and contains **no OpArrayLength**. Shader B uses runtime `.length()` and does contain OpArrayLength. Their descriptor bindings/types/read-write qualifiers match. The fixture creates sets using shader A and actually dispatches B, recreates all buffers/sets with different sizes for B, then creates sets using B and actually dispatches A. The same shader/pipeline objects survive the rounds; old resource cleanup requests occur only after all four completed readback callbacks for a round. No physical-free observation is claimed.

Original Metal logical-size inputs remain strict: 20-byte uint storage expects 5 elements, 40-byte prefixed storage expects 6 tail elements, 48-byte vec4 storage expects 3, output16 expects 4. The recreation case uses 28/24/16/16 bytes and expects `[7,2,1,4]`. These `.length()` expectations remain **unexecuted on Metal** because the old host fails native compilation first. They are not adjusted to GPU-observed values.

Vulkan has a different existing driver rule: `RenderingDeviceDriverVulkan::buffer_create` rounds p_size to at least 16 bytes before storing original_size (`drivers/vulkan/rendering_device_driver_vulkan.cpp:1958–1974`), and the storage descriptor range uses buf_info.size (`:5636–5642`). Therefore the nonaligned Metal logical-size oracle is not used for Vulkan. Vulkan was run with an explicit separate aligned profile: 32/32/48/16 bytes expects `[8,4,3,4]`; recreated48/48/32/16 expects `[12,8,2,4]`. These expected arrays were fixed in the input builder before execution and are independent of GPU observations. **No Vulkan nonaligned run was made.**

## Actual runs

| Run directory | Command options after run.py | Actual result |
| --- | --- | --- |
| `runs/20261104T042145Z-593b92c2-vulkan` | `--backend vulkan --aligned` | Vulkan1.2.357, Forward Mobile, Apple M3 Pro; exit0; 3/3 full-buffer literal checks pass, 12 completed readback callbacks. A→B and B→A set reuse plus recreate are exercised. |
| `runs/20261104T042204Z-3831689e-metal-control` | `--backend metal --control-only` | Metal4.0, Forward Mobile, Apple M3 Pro(Apple9); shader A only; exit0; actual read/write control passes with four completed readback callbacks. This proves no-length active bindings work, not that runtime lengths work. |
| `runs/20261104T042215Z-90b36161-metal` | `--backend metal --expect-metal-aux-red` | Shader A native pipeline is ready; shader B native pipeline fails with the exact auxiliary collision below; host exit2; no dispatch/readback row accepted. The launcher exits0 only because this specific expected RED is detected. |

Exact old Metal diagnostic: generated MSL line51 reinterprets `spvDescriptorSet0.spvBufferSizeConstants` to `device ActiveZero* constant &`, casting away qualifiers; a second diagnostic shows set1's `vectors` similarly aliased to `spvDescriptorSet1.spvBufferSizeConstants`. The expected-RED check requires A pipeline ready, B pipeline failure, the auxiliary name, the qualifier/duplicate-field diagnostic family, exit2 and an empty failed result. A generic crash, timeout or unrelated ERROR does not pass that oracle. Complete raw errors remain in host.log and RESULT.json; they are not hidden or called a successful Metal execution. No RID leak warning appears in these runs.

The Vulkan log retains one `OpArrayLength is not supported yet.` line. It comes from optional re-spirv parsing (`thirdparty/re-spirv/re-spirv.cpp:1861`); driverVulkan skips that optional optimization and uses the original SPIR-V when parsing returns an empty shader (`:5180–5220`). This is retained diagnostic context; finite actual readback results above are unchanged.

## API and acceptance limits

Operations execute through RenderingServer.call_on_render_thread on the actual main RD; no local RD or main-RD submit/sync is used. Results are accepted only after buffer_get_data_async callbacks, not after a frame count or RenderingServer CPU sync. Readbacks introduce observation/synchronization and do not establish a no-stall schedule. Public free_rid calls are cleanup requests only.

Dynamic storage/ring-offset controls remain **deferred**. The exposed public compute_list_bind_uniform_set takes list/set/index, not dynamic offsets; RDUniform exposes resource IDs without dynamic slice offsets. This fixture invents no interface to reach the private driver path. Direct/Tier1, private dynamic rebasing, all-platform limits and actual physical disposal remain untested here.

This package tests finite GPU length/address and compatible-set mechanisms. It grants no E09 source authority, producer token, C++ guard-wrapper qualification, renderer draw/adoption, completed resource retirement, whole B4/ANGE or repaired backend acceptance.
