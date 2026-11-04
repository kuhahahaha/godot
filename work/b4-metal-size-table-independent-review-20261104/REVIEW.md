# Independent Metal buffer-size helper review

Reviewer `/root/b4_guard_wrapper_review`; author `/root/b4_instance192_review`. Scope: the pure `drivers/metal/metal_buffer_size_table.h` helper, its author model, and the buffer descriptor-array refusal condition. No author production source was edited. The runtime-control package and old guard evidence were left unchanged.

Result: **no source defect found in this scoped helper review**. This does not sign off the Metal argument ABI, driver/cache/resource-lifetime implementation, dynamic binding integration, native compilation or repaired-host GPU behavior; root reviews the driver/container integration separately.

Reviewed helper SHA256: `7eaefef3ae33ad04ea4dc7bf498e73f93451863c29b1ff58cbcb003f9d4358a1`. The exact header and author model are preserved here. The descriptor-array condition is extracted exactly from current production source and hashed independently in `REFLECTION_REFUSAL_RESULTS.json`.

## Layout and checked writes

For N dense native argument resources, ordinary pointer entries occupy `[0,8N)`, the auxiliary pointer is at `8N`, size words begin at `8(N+1)`, and the allocation ends at the next multiple of eight after N four-byte size words. Initialization uses widened arithmetic before addition/multiplication and checks the final allocation against INT32_MAX before narrowing. Rejected initialization preserves the previous layout.

The largest accepted N is independently fixed at 178956969: sizes_offset1431655760, allocation_size2147483640. N178956970 refuses. The boundary test checks layout arithmetic only; it does not allocate a two-gigabyte buffer. Zero and UINT32_MAX inputs refuse.

write_length refuses out-of-range native resource indices and byte lengths above UINT32_MAX before memcpy. It preserves the exact supplied logical byte length, including zero and UINT32_MAX; it does not round to a native stride or multiply by a frame count. write_address requires eight-byte GPU-base/allocation-offset alignment and separately checks offset+table-offset and base+that-sum for overflow before writing only the auxiliary pointer slot. Both helpers require the internal caller to provide valid CPU storage of allocation_size bytes; they are not general external-pointer validators or allocation owners.

The independent `boundary_test.cpp` directly includes the production header and checks:

- 1024 small layouts with independent offset/extent assertions and prefix/suffix canaries; length writes leave ordinary pointers, the auxiliary pointer and padding untouched.
- Rejected index/length writes produce no partial mutation.
- Static and offset-based self pointers land at GPU-base+allocation-offset+sizes_offset and change only their own eight-byte slot.
- 144 combinations of base/offset are compared with an independent unsigned-128-bit sum oracle, including alignment and UINT64 overflow boundaries.
- Independently allocated tables do not share pointer/size data.

The author model, the independent boundary model, and the independent model under ASan+UBSan all compile and exit0. Commands, exit codes and raw-log hashes are in `RESULTS.json`. No engine/native runtime is represented by these CPU executions. In particular, the author model's logical20/stride32/ring96 example is a scalar example; actual driver assignment of logical_size and dynamic command-buffer rebasing are outside this helper test.

Three targeted mutations are independently detected by the intended assertion, not by an unrelated compiler failure: placing the size table over the auxiliary pointer slot, dropping the per-allocation dynamic offset from the self pointer, and allowing oversized byte lengths to truncate. Exact substitutions, candidate hashes, failure expressions and raw-log hashes are in `MUTATION_RESULTS.json`; `run_mutations.py` reproduces them without editing production files.

## Descriptor-array refusal boundary

`check_reflection_refusal.py` compiles and validates actual GLSL/SPIR-V and uses the repository SPIRV-Reflect implementation to inspect descriptor dimensions. It evaluates the **exact extracted production refusal expression**, with only enum/type plumbing supplied by the harness. It does not execute the full Metal container.

A runtime array *inside* one storage block correctly has descriptor array dims0 and is allowed. Outer storage descriptor arrays of one and two elements, and an outer uniform-buffer descriptor array, have dims1 and are refused. Suppressing the dims check causes all three negative cases to fail the same exact oracle. Results include the input GLSL, actual SPIR-V hashes, reflection fields and the extracted condition identity in `REFLECTION_REFUSAL_RESULTS.json`.

This checks that the existing one-RID buffer-binding interface is not silently interpreted as descriptor-array support. Dynamic descriptor binding and actual native Tier1/direct execution is not exercised here (the production refusal slice is replayed below). Dense resource indices versus source binding numbers, mixed-resource MSL encoder allocation, compatibility/cache identity and actual tail-pointer residency must still be checked at the integration call sites; a correct pure helper does not establish those facts.


## Additional production-slice translation replay

After the author supplied its stable source-slice harness, seven selected controls were rerun independently using the author's already-built translation tool, with every output redirected into this review directory. The exact tool/input hashes, commands, expected/actual exits and stdout/stderr are in `TRANSLATION_REPLAY_RESULTS.json`; the reviewed harness and generated source slices are copied here. This review did not rebuild that translation tool or an engine. `CURRENT_SOURCE_MATCH.json` confirms that all three copied slices occur verbatim in current production source and that the helper still matches its reviewed snapshot.

All seven exits and targeted checks match: descriptor-array refusal3 with its exact refusal message; mixed sampler-array resources allocate four slots before storage and auxiliary ID5; multiple sets translate with distinct auxiliary mappings; merged vertex/fragment reflection uses the shared union auxiliary ID2; actual direct-length refusal slice returns4; direct no-length translation returns0. The positive MSL outputs have the expected auxiliary IDs, not merely successful compiler exits.

These runs combine actual repository SPIRV-Cross translation with verbatim production auxiliary/refusal slices and the pure helper. Surrounding reflection/type plumbing remains a harness; they are not execution of the complete RenderingShaderContainerMetal, native MSL compiler, argument encoder, dynamic driver path or GPU. No broader ABI/GPU acceptance is implied. No further finding arose from these scoped controls.
