# Metal guard failure: confirmed auxiliary buffer ABI gap

Status: **root cause confirmed; production fix not implemented**. Reviewer `/root/b4_instance192_review`; coordinated actual-container export with `/root/b4_guard_wrapper_review`. No production source changed, no engine build, no second host launched by this reviewer. The GPU author used the existing frozen a2 host (`4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528`) for a root-authorized, translation-only export; it created no pipeline and dispatched no GPU work.

## Conclusion

This is a Metal `OpArrayLength` integration gap, not an invalid B4 shader or malformed `RDShaderSPIRV` fixture. Godot explicitly maps ordinary descriptor bindings into dense Metal argument IDs starting at zero, but neither assigns an independent ID to SPIRV-Cross's synthesized buffer-size table nor supplies that table at runtime. SPIRV-Cross assigns the otherwise-unmapped auxiliary table argument ID zero and aliases it with the first real storage buffer. The generated constant/device pointer reinterpret_cast is the exact compiler error in the main-RD failure log.

**Unused descriptors are not the cause.** VALIDATE actively reads CurrentRecords and still has the same collision. Removing record descriptors from INIT changes the collision to OutputMetadata, which INIT actively reads. Deleting `.length()` or merely reserving another argument ID would hide or incompletely fix the failure; correct runtime byte-length data and lifetime support are required.

## Exact evidence

The actual host-produced containers are `*.metalbin`; `extract_msl.py` decompresses their Zstd stage data and verifies the embedded Metal StageData SHA-256 against the extracted source. `MSL_EXTRACTION.json` records all six containers, source digests and relevant lines. This is actual engine-generated MSL, not handwritten substitute code.

| Actual generated source | Auxiliary declaration | Colliding real buffer | Invalid alias |
|---|---|---|---|
| `INIT.metal` | line103, `spvBufferSizeConstants [[id(0)]]` | line104, CurrentRecords id0 | line636, exactly matching original host diagnostic |
| `VALIDATE.metal` | line164, id0 | line165, active CurrentRecords id0 | line879 |
| `FINALIZE.metal` | line87, id0 | line88, CurrentRecords id0 | line102 |
| `INIT_EMPTY.metal` | line93, id0 | line94, active OutputMetadata id0 | line628 |
| `FINALIZE_EMPTY.metal` | line77, id0 | line78, OutputMetadata id0 | line90 |
| `INIT-validate_only_records.metal` | line93, id0 | line94, active OutputMetadata id0 | line624 |

The record-gating candidate is artifact-only: both `#ifndef MODE_EMPTY` record declaration/reader regions were changed to `#if defined(MODE_VALIDATE) && !defined(MODE_EMPTY)`. All five candidate SPIR-V modules passed glslang and spirv-val. The original/candidate SPIR-V resource and OpArrayLength listing is in `SPIRV_INSPECTION.json`; exact disassembly is retained. Every mode still has OpArrayLength (at minimum scratch/status). INIT original and candidate both have nine instructions; VALIDATE four; FINALIZE two. Therefore even fully pruning unused descriptors cannot remove the size-table requirement.

Actual generated reads demonstrate the required ABI: INIT uses `spvDescriptorSet0.spvBufferSizeConstants[4]` for scratch bytes, `[5]` for status, `[6]` commands, `[7]` proof, `[2]` metadata and `[3]` source. Runtime array lengths are then calculated from byte lengths: proof `(proofBufferSize - 224) / 144`, words `(bufferSize - 0) / 4`. The table indexes are **Metal resource indexes, not original Vulkan binding numbers** (the empty/candidate layouts use dense0..5 for source bindings2..7).

The generated kernel also contains a `constant uint* ... [[buffer(25)]]` parameter. Actual argument-buffer-path size expressions dereference the auxiliary member of `spvDescriptorSet0`; supplying only global slot25 would not repair this path.

## Source chain

1. `servers/rendering/rendering_shader_container.cpp:385–545` reflects all declared descriptor bindings; no inactive-resource filter removes them. They are sorted by binding at705–707.
2. `drivers/metal/rendering_shader_container_metal.cpp:474–475` sets `force_active_argument_buffer_resources=true` and padding. At526–532 it initializes a dense per-set argument counter. At638–650 each buffer receives an explicit MSL mapping. At748–755 those mappings and push constants are registered with SPIRV-Cross. There is no `kBufferSizeBufferBinding` mapping or length-table metadata.
3. `thirdparty/spirv-cross/spirv_msl.cpp:15649–15756`: explicit mapped resources return their mapped index immediately. They do not advance `next_metal_resource_ids`. That array starts at zero; the unmapped-resource path allocates from it.
4. `spirv_msl.cpp:19643–19651` creates a UInt pointer named spvBufferSizeConstants with sentinel `kBufferSizeBufferBinding`, then asks `get_metal_resource_index` for an index. With no mapping it gets0. At19708+ equal indexes are marked overlapping; alias emission generates the illegal constant/device reinterpret_cast. This is faithful behavior for the conflicting remaps supplied by the engine.
5. The runtime side is also absent: `rendering_device_driver_metal.cpp:1500–1553` sizes argument buffers from ordinary reflected resources only. `uniform_set_create` (1617+) writes actual resource GPU addresses (storage buffer case1720+) and does not create/fill an auxiliary size table. `MDUniformSet` (`metal_objects_shared.h:971`) has no size-table ownership. Changing only the emitted auxiliary index would leave runtime shader reads pointing at an absent/uninitialized table.

## Minimal honest backend repair slice

A correct repair can leave the B4 shader and its length guards intact. It needs all of the following, not just a shader-text change:

- **Translation and metadata:** detect OpArrayLength requirements, reserve a non-overlapping auxiliary slot per argument-buffer set using `kBufferSizeBufferBinding`, retain each required table length/index relationship, and serialize that metadata. Update Metal container format/cache compatibility so old bytes cannot be loaded with a new layout. The source and reflection must agree for multi-stage and shared uniform-set layouts.
- **Binding-visible byte lengths:** populate the table with the exact bound storage-buffer byte ranges indexed by the emitted Metal resource indexes. Do not use heap `allocatedSize()` or total persistent-ring size. `BufferInfo` presently stores no original logical size; dynamic `size_bytes` is rounded frame stride. Preserve original descriptor-visible byte size where needed, and handle the chosen dynamic frame/range without inventing more accessible bytes. Validate narrowing to the table's u32 entries.
- **Allocation, binding and retirement:** give the native uniform-set owner a retained size-table buffer; include its address in the argument table, actual resource usage/residency tracking, upload ordering, failure unwind and existing deferred disposal. The table is immutable per retained uniform-set/binding snapshot and survives its final GPU use. Dynamic uniform-set copies and render/compute bind paths must preserve the pointer and relevant per-frame range.
- **Supported profiles:** argument-buffer Tier2, direct/Tier1 slot binding and dynamic buffers are different branches. Implement the admitted branches, and explicitly refuse unimplemented ones before pipeline publication. Direct binding needs its own reserved global auxiliary slot and binding logic; setting the default slot25 only in options is insufficient. A finite Tier2/static-compute-only qualification can be explicit, but must not advertise generic OpArrayLength support.

Likely owned files: `rendering_shader_container_metal.{h,cpp}` for mapping/reflection/container version; `metal_objects_shared.h` and `rendering_device_driver_metal.{h,cpp}` for size metadata, allocation and lifetime; `metal3_objects.cpp` for binding/resource tracking and dynamic handling. The allocator may remain reusable, but its existing resource ownership/retirement contract must be followed. No thirdparty algorithm change appears necessary for this collision: the engine must provide a complete, nonconflicting mapping and actual table.

## Targeted red/green requirements

1. Preserve the original INIT runtime pipeline compile failure as RED and the hash-verified generated collision sources. Add an active-binding0 runtime-array shader control so pruning inactive declarations cannot fake GREEN.
2. After a backend implementation, inspect actual generated MSL: independent auxiliary ID, no overlap alias, table indexes match serialized mappings. Create all five unchanged guard pipelines on actual Metal.
3. A minimal Metal compute fixture reads array lengths for differently sized SSBOs (including nonzero binding numbers and gaps), a struct prefix plus runtime array, and multiple sets. Assert exact element counts. For admitted dynamic buffers verify logical per-frame size, never ring/padding extent. Exercise buffer recreation/new uniform-set generation and resource retention until actual completion.
4. Repeat actual guard healthy, zero-capacity, source-invalid/count-invalid, bounds/history cases plus the source-validity mutation and restored healthy controls. Keep numeric safe-math/platform qualification separate from mere pipeline creation. Partial table allocation or upload failure must leave no published usable pipeline/set and must preserve cleanup obligations.
5. Re-run Vulkan controls because the shader is unchanged; do not substitute those results for Metal.

## Fixture/API and uniform-superset findings

The fixture sets the compute-stage bytecode in a new RDShaderSPIRV and calls main-RD `shader_create_from_spirv` / `compute_pipeline_create` on the render thread. There is no user-provided Metal remap/length metadata omitted from this API: the driver owns translation. All original bytecode modules passed spirv-val. The actual source at the exact failing line confirms the bytecode reaches the expected driver path.

Public `uniform_set_create` accepts a provided-uniform superset for ordinary bindings: `rendering_device.cpp:5050–5075` loops reflected set entries, finds each supplied binding, and sizes `driver_uniforms` to the reflected count. Extra supplied bindings are not forwarded to the driver. Earlier qualification/dependency scans still examine supplied RIDs, so this is not permission to pass arbitrary invalid/foreign handles. The B4 pure layout planner's retained binding superset therefore need not be trimmed solely to satisfy normal reflection; that fact does not repair the auxiliary-table gap.

## Vulkan diagnostic attribution

`OpArrayLength is not supported yet.` comes from optional re-spirv optimization: `thirdparty/re-spirv/re-spirv.cpp:1860–1863` prints an unsupported opcode and returns false. `drivers/vulkan/rendering_device_driver_vulkan.cpp:5180–5220` leaves `decoded_spirv` unchanged when that parser is empty and continues to vkCreateShaderModule. It is not a Vulkan driver statement that runtime-array length is unsupported. The GPU author's actual Vulkan output/control results remain evidence for Vulkan; logs should retain this precise fallback attribution, not globally suppress diagnostics or recast it as Metal success.

## Verification limits

The source-derived collision prediction was independently confirmed by six **actual engine-translated** MSL exports. The original Metal pipeline failure is a real runtime compiler result supplied by the GPU author. A separate attempted `xcrun metal -fsyntax-only` check was **UNAVAILABLE**: the wrapper exists, but Xcode reports the Metal Toolchain component missing. `MSL_DIAGNOSTIC_CHECKS.json` records that environment failure; it is not a shader RED and no manually corrected MSL control was validated. No toolchain installation was attempted. No backend fix, rebuilt-host test or successful Metal guard GPU run has occurred.
