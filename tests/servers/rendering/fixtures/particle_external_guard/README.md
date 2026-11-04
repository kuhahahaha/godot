# Instance192 protected command guard component fixture

Run from the Godot source root:

```sh
python3 tests/servers/rendering/fixtures/particle_external_guard/run.py --mutations
```

Requires `glslangValidator`, `spirv-val`, `spirv-dis` and `clang++` on PATH. Temporary SPIR-V and CPU binaries are removed after the run. No Godot build is launched.

The fixture compiles and validates the production shader's INIT, VALIDATE and FINALIZE variants, reflects its eight bindings, 32-byte push constants, 224-byte proof header and 144-byte surface stride, and checks `NoContraction` on validation arithmetic. It extracts the production functions into a scalar C++ harness rather than maintaining a second copy of guard conditions. The harness runs 40 sequential control/interval cases. Additional cases cover distinct indexed/nonindexed surface commands, a second-surface fault zeroing every command, 64/65/129 capacity chunks with padded lanes and poisoned inactive tails, forward/reverse lane and chunk ordering, and both actual source-validity interpretations. These cases exercise scalar invocation ordering rather than claiming concurrent GPU execution.

Three intentional source mutations must fail the corresponding independent expectation: ignoring actual source validity, dropping geometric bounds faults, and skipping previous-history validation.

This is component evidence. Scalar lanes do not prove GPU visibility or scheduling. It does not run the E09 source bridge, material shaders, renderer draw consumption, adoption, completion or retirement. The wrapper must record INIT, barrier, every capacity-bounded validation chunk, barrier, FINALIZE, then a barrier before command consumption. It must retain all original backings and immutable proof/material/source associations. Missing chunks are a caller contract failure, not something the shader can infer from a count word.

The shader accepts only Instance192 data. No Indexed144, SceneOnly, double-precision or production qualification capability follows from compiling it. F32 inputs, finite and non-subnormal values, reserved bits, eligibility, history identity, source count equality, metadata, and current/valid-previous bounds are checked. Transform classification, normal/inverse generation, determinant threshold decisions and material displacement recipes still require their original qualified producer and immutable owner proofs; this shader does not independently sign those algorithms.

Binding 3 must be the retained real source backing. `GeometryProof.source_accessor` describes word offsets and the original validity interpretation (1: initialized word nonzero, 2: fault word zero). These private component parameters do not grant authority or create an E07 publication. The production bridge must reject an unsupported or missing real accessor before recording and must preserve the complete source validity obligations; a single flag cannot replace those obligations.

Scratch words 0–7 capture OutputMeta32 once during INIT. Word 8 is the aggregate atomic fault, word 9 is the minimum failure tuple `instance*16 + previous_domain*8 + surface`, word 10 is immutable INIT fault, and 11–15 are zero. Metadata/source preflight faults use tuple 0 and are distinguished by word 10. Record numeric/layout failures use surface 0. VALIDATE reads only word 10 to skip globally invalid inputs; a record fault never causes another lane to skip and lose its independent fault bits. Status is not observable as completed merely because INIT wrote fault zero.
