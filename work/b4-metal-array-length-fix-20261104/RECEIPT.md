# Metal runtime-array byte-size table implementation

Status: implemented in seven Metal backend source files; source checks and standalone controls pass. **No repaired-engine Metal pipeline or GPU success is claimed.** No engine build or new host was started for this implementation. Production guard GLSL, old host and previously committed diagnosis/runtime-control evidence are unchanged. Changes remain uncommitted for root review.

The starting RED is the actual old-host main-RD Metal native-pipeline failure recorded in `../b4-guard-metal-diagnosis-20261104` and `../b4-metal-array-length-runtime-control-20261104`. Explicit ordinary argument remaps left the compiler's synthesized size-table resource at ID0, colliding with a real buffer, and the driver supplied no size table. Removing unused guard declarations could not repair the ABI. `RED.md` links the original evidence.

## Implementation and scope

- The container maps SPIRV-Cross's synthetic buffer-size resource to native argument ID N, after all N ordinary argument entries. Reflected sets with any storage buffer receive this fixed layout whether or not their creating shader uses length. Both directions of same-layout shader/set reuse therefore have the same reserved pointer and table; stage activity does not determine allocation. Stage mappings use the merged reflected set layout.
- `MetalBufferSizeTable` defines ordinary entries `[0,8N)`, auxiliary pointer at `8N`, uint32 byte lengths at `8(N+1)`, and an eight-byte-aligned allocation end. Arithmetic is widened and checked before narrowing; invalid length/index/address writes fail before mutation. The allocation is bounded by INT32_MAX, matching the existing argument byte vector. Reflected resource extents are checked too.
- `BufferInfo.logical_size` retains original requested bytes before allocation rounding or dynamic ring expansion. Uniform-set creation writes those bytes at each buffer's remapped native index. This is the Metal descriptor-visible range chosen by this fix. Vulkan currently exposes a rounded allocation range; it was not changed, and no common cross-backend length semantics are claimed.
- Pointer and size words occupy the original argument allocation tail. Static self pointers use that MTLBuffer address; render and compute dynamic copies rebuild the pointer from the current scratch buffer GPU address plus allocation offset. They retain logical byte lengths rather than frame stride or ring size. Existing argument-buffer upload, ownership and retirement apply to the entire allocation. The containing native buffer is explicitly marked read for the indirect table access.
- Uniform count/CPU allocation and static native allocation failures unwind before publication. A failed static self-pointer write returns the native allocation before freeing the set. No extra independently owned GPU allocation was introduced.
- Direct/Tier1 length use is explicitly refused during container creation. Ordinary direct shaders without length remain allowed. Buffer descriptor arrays are explicitly refused because the existing RD buffer binding API accepts one RID; a runtime array inside one buffer block remains allowed. This does not add a descriptor-array API or claim generic Tier1 support.
- Metal container format is now 4; existing version gate rejects format3 caches for regeneration. The aux layout follows reflected storage layout, independent of per-stage active resources.

`OWNED_FILES.txt` enumerates the exact seven production files and `verification.json` locks their SHA256. Root separately reviews driver/cache/lifetime integration.

## Executed validation

Reproduce standalone checks from the repository root with:

```sh
python3 work/b4-metal-array-length-fix-20261104/verify.py
```

`BUILD_AND_RUN_RESULTS.json` records exact commands, stdout/stderr and successful exits. `CHECK_RESULTS.json` contains 26 matched expected outcomes:

- Production helper boundary model, including zero, maximum accepted N178956969/first rejected successor, UINT32 index/length limits, no partial mutation, multiple sets, same-layout reuse and logical20 versus stride32/ring96 examples; pointer alignment and UINT64 overflow checks.
- Actual repository SPIRV-Cross translation for all five original guard SPIR-V variants: original ordinary remaps reproduce the aux collision; the production auxiliary-mapping slice produces distinct aux IDs.
- Multiple sets with nonzero source bindings, mixed sampler-array resources preceding storage (aux ID5), and merged vertex/fragment reflection (aux ID2 in each stage).
- Actual production early-refusal slices reject legal upstream buffer descriptor arrays and direct length use; direct no-length remains translatable.

The harness uses the actual production helper and three verbatim source slices locked in `SOURCE_SLICES.json`. Its surrounding resource reflection/mapping plumbing is a standalone harness, not execution of the complete Godot container. Positive outputs are translated MSL, not native Metal compiler or GPU acceptance.

`CACHE_GATE_RESULTS.json` records an independently compiled exact production header/version gate: real old format3 container rejected; synthetic format4 prefix accepted at the gate; future format5 rejected. This is not a full format4 serializer/deserializer round trip.

`SYNTAX_RESULTS.json` records clang `-fsyntax-only` on the three modified Metal translation units, all exit0, with final source hashes verified against disk. Generated headers were supplied from the existing qualification worktree as recorded in the commands. No engine linking occurred. `git diff --check` passed.

Independent review `../b4-metal-size-table-independent-review-20261104/REVIEW.md` found no defect in its declared helper/refusal scope. It adds 1024 canary layouts, 144 unsigned-128-bit address-oracle cases, ASan+UBSan, three targeted helper mutants, actual SPIRV-Reflect descriptor-dimension controls plus targeted refusal mutants, and seven selected source-slice translation replays. This scoped review does not sign off complete driver/native/GPU behavior.

Built standalone executable hashes are retained in `BUILT_BINARIES.json`; the executables are omitted from the evidence package and rebuilt by `verify.py`. Source, generated MSL/SPIR-V, commands and results remain available.

## Outstanding validation

A newly built engine must run the existing independent Metal runtime controls and original guard controls. Required observations include exact non-aligned logical lengths, both directions of shared-set reuse, recreated buffer/set sizes, complete native container round trip, native pipeline creation and real dispatch/readback. Actual dynamic ring binding, resource retirement under GPU execution and injected allocation-failure paths have not run. No Metal toolchain compilation, new-host execution or successful repaired Metal GPU result is included here. Direct/Tier1 length and buffer descriptor arrays remain intentionally unsupported profiles with explicit admission failures.
