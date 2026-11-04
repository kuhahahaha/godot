# Instance192 dual-renderer shader implementation — component evidence

The RD scene shaders now have a real Instance192 variant family, pipeline-key/hash and input-mask selection, current/previous packet bindings, per-record material transforms and shared fragment culling. This receipt does not qualify a production SourceBridge, renderer draw adoption, backend winding, GPU motion/history or physical retirement.

## Exact source ownership

Only these nine production files belong to this change; root owns the accompanying spatial builtin registration and GLES3 mapping separately:

- `servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h`
- `servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp`
- `servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h`
- `servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.cpp`
- `servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl`
- `servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered_inc.glsl`
- `servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile.glsl`
- `servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile_inc.glsl`
- `servers/rendering/renderer_rd/shaders/instance192_inc.glsl`

`verification.json` records their exact SHA-256 values and all local evidence. No commit was made by this subagent. Other dirty RD and builtin files are preserved.

## Internal caller interface

`PipelineKey.instance192` defaults false. The existing shader/mask getters append `bool p_instance192=false` after `p_indexed144`; the two profiles are mutually exclusive. The third variant family preserves the old Native and Indexed144 indices and is enabled lazily. The C++ structural material gate refuses double builds, native trails/point-size and unqualified position/model/projection/skip/world-coordinate modes. This gate is not a material qualification or displacement proof.

Set2 binding0 is current `Transforms`, twelve vec4 slots/192 bytes per record; binding1 is complete aligned previous data. Index is `gl_InstanceIndex`; the External vertex path does not add the scene instance index or multiply the Host again. It installs record model/normal/inverse, color and INSTANCE_CUSTOM before user VERTEX. Mesh CUSTOM0..3 keep their native attribute streams. Current and previous records are selected explicitly; invalid history does not fetch stale previous records and writes positive-zero motion at the final output.

Forward+ reuses location10 as `uvec3(scene_index,record_index,flags)` only for External; Native keeps its existing uint. Mobile uses the free location10 uvec2. Author varyings still begin at15. Both vertex and fragment `INV_MODEL_MATRIX` use the same private inverse variable: ordinary Native uses `inverse(read_model_matrix)` and Instance192 uses its record inverse. Its usage also defines MODEL_MATRIX_USED for the Native double/multimesh composition path. Root owns the readonly ShaderTypes and GLES3 additions.

Hardware cull is disabled for Instance192. Specialized shaders receive original RD material/pass cull in specialization4; ubershaders use the existing dynamic `uc_cull_mode()` push field (their pipeline key intentionally carries disabled cull). The logical front is raw front XOR per-record mirror. Caller cull basis includes the actual pass orientation, but must not flip Host/record determinant a second time: record C and its mirror already include Host. Ordered records are never parity-bucketed. `cull-literal-results.json` contains the literal 24-row specialized/uber policy table. These CPU literals do not calibrate actual backend winding.

SceneData current/previous camera and time remain caller bindings. The exact history tuple must be supplied or refused by that caller; global previous-frame data is not proof for a skipped view. Material recipe/bounds/inverse-validity proof and camera/source identity remain original-owner obligations.

## Actual checks

```sh
python3 work/b4-instance192-shader-20261104/compile.py
python3 work/b4-instance192-shader-20261104/negative_and_native.py
python3 work/b4-instance192-shader-20261104/cull_literal.py
```

- 272 actual complete renderer shader stages compiled with glslang and passed spirv-val; 136 V/F pairs linked. Native/External, specialized/uber, depth/dual-paraboloid/material/normal-roughness/voxel/SDF/color/alpha-hash/motion/lightmap/multiview/fog/FP16 and Native-trails variants are covered. Every pair really uses author varyings15/16. Inverse cases actually use the new inverse in VERTEX and FRAGMENT control flow; FRONT_FACING and all mesh CUSTOM channels are exercised. These are native templates with explicitly injected generated material snippets, not an execution of the engine ShaderCompiler frontend.
- 32 additional checks passed: 24 Native color/motion/trails preprocessed token streams match the recorded baseline commit; double/profile-alias refusal4; old material-specific trails-error global-group target RED2; author-varying15 collision target RED2.
- Both actual cull helper functions ran on CPU for specialized/uber with deliberately different static/dynamic cull values: 24 rows/48 assertions passed. Replacing the uber dynamic read with the old static-only code produced the expected failure (8 mismatches), not a generic crash.
- Both SceneShader translation units passed `clang++ -fsyntax-only`; exact commands, stdout/stderr and return codes are in `syntax-results.json`. Only frozen a2 generated/include fallback was read; it was not modified. No scons, engine build or GPU run was started.

## Native isolation correction and limits

ShaderRD::enable_group schedules the newly enabled group across every existing material version; new/modified versions also compile enabled groups. `_compile_version_end` clears all enabled variants of a material version if one group fails. Therefore a material-specific `#error USE_PARTICLE_TRAILS` was incorrect even when direct Instance192 requests are refused. The fixed External trails variants compile successfully but remain unselectable through both C++ shader/mask getters. Native trails token comparison and actual full-template compile/link controls accompany the old-error RED. Actual engine group lifecycle and direct C++ refusal runtime tests were not run.

Independent Astra source review found the global-group issue and verified the component correction with additional real shader compiles; it retained the Engine/GPU limits. The actual backend mirror/cull calibration, complete material recipes (including LateAlpha), full engine build, production source association, exact packet camera/history caller, real draw and physical return remain pending. Successful template compilation is not a capability certificate.
