# Independent guard wrapper review

Reviewer: `/root/b4_guard_wrapper_review` (independent from author). Source base: `8355b9a7ae2fb5e0ceaaa8453021865df16add62` plus uncommitted wrapper. The reviewed pre-fix header/source/shader are preserved beside this report, with hashes in `zero-capacity-counterexample.json`.

Final status: the one source/layout finding is fixed and independently rechecked. No further source findings in this reviewed component. Initial review status was changes required. No engine build, real native pipeline preparation, uniform-set creation, dispatch, GPU ordering or physical retirement was executed.

## Finding 1 — zero-capacity layout cannot satisfy its shader

At pre-fix `particles_external_guard.cpp:173–176`, `plan_layout` skips null record RIDs. For legal capacity zero, bindings 0/1 must be absent and the planner returns OK with only bindings 2–7. The actual INIT and FINALIZE shader variants nevertheless declare and retain bindings 0/1 in SPIR-V. `RenderingShaderContainer::_reflect_spirv` enumerates every reflected descriptor (`servers/rendering/rendering_shader_container.cpp:383–545`), and `RenderingDevice::_uniform_set_create_qualification` requires each shader binding to be supplied (`servers/rendering/rendering_device.cpp:5064–5074`). The nominally successful zero plan therefore cannot create its real uniform set. Adding a fake one-record allocation would violate OUTPUT_CONTRACT §4's explicit empty packet requirement.

`python3 work/b4-guard-wrapper-review-20261104/zero_capacity_counterexample.py` executes the exact snapshotted production planner body using transport-only RID/vector/uniform shims, then compiles and validates every snapshotted production shader variant and compares actual reflected binding numbers. Observed: planner OK, supplied `[2,3,4,5,6,7]`, all three shader variants require `[0,1,2,3,4,5,6,7]`. This is a CPU/component counterexample with source-backed RD refusal, not a claimed native RD test. Script exit zero means the known counterexample was detected.

Suggested minimal fix: dedicated INIT_EMPTY/FINALIZE_EMPTY layouts that compile out record blocks and record reader functions, explicitly selected by the zero-capacity plan. They must preserve real source/meta validation and protected zero commands/status. All fixed pipelines must be actually prepared before reporting ready. Keep the nonzero three-stage schedule and no record descriptor/dummy allocation in the empty profile.

## Other inspected paths

The capacity/surface bounds make all products/additions fit u64/u32 as used; max-group multiplication is widened before multiplying by 64 and chunk coverage strictly progresses. Request subsets/reserved bits refuse before output mutation; output is assigned only after complete local construction. RID equality rejects direct aliases; actual backing authority remains expressly the original owner's responsibility. Layout preparation does not create uniform sets or append graph operations.

Pipeline creation is main-RD/render-thread restricted and returns ready only after every actual shader/pipeline succeeds. Failure clears already allocated pipeline RIDs in reverse order and then the shader version; RD pipeline validity here means owner membership, so the cleanup predicate does not conceal a live invalid backend object. Wrong-thread destruction intentionally retains native objects rather than invoking ShaderRD frees off-thread; the owner must release correctly. This is explicitly an owner discipline requirement, not independent retirement evidence.

The five added doctests were read but not run. The original author receipt correctly disclaims engine/GPU/native pipeline execution and production authority. An independent fixture run overlapped root's active script update and is discarded as a mixed-version run; a stable post-fix rerun is required.


## Post-fix independent recheck

Root implemented explicit INIT_EMPTY/FINALIZE_EMPTY variants with no record declarations or reader functions and selects those modes in the zero-capacity `LayoutPlan`. `prepare_pipelines` requires all five actual variants/pipelines before ready. Empty INIT refuses nonzero capacity with COUNT_FAULT. The remaining owner-supplied source/meta/proof descriptors and protected stores remain present.

The immutable `post-fix/` snapshots match the six current production/test sources exactly; identities are in `post-fix/reviewed-sources.json`. Independently executed:

- `python3 work/b4-guard-wrapper-review-20261104/post-fix/capacity_layout_regression.py`: exact production planner body returns OK, selects both EMPTY modes, has no validation chunks, supplies bindings 2–7. Both actual compiled/validated EMPTY SPIR-V variants require exactly 2–7; no missing descriptor. Raw result: `post-fix/capacity-layout-regression.json`.
- `python3 work/b4-guard-wrapper-review-20261104/post-fix/run.py --shader work/b4-guard-wrapper-review-20261104/post-fix/particles_external_guard.glsl --mutations`: exit 0. All five production SPIR-V variants compile/validate/layout pass; the existing 40 sequential CPU production-control/interval cases pass and all three targeted source-validity/bounds/history mutations are detected. Raw result: `post-fix/shader-fixture.log`.
- Both author-recorded clang syntax commands rerun independently: DEBUG/TESTS and release each exit 0, empty diagnostic logs. Commands and result/log hashes: `post-fix/syntax-results.json`.

Finding 1 is closed for source/layout correctness. Actual engine doctests, native pipeline/set preparation, GPU dispatch/order, E09 source authority, draw adoption/completion and physical retirement remain unexecuted here and are not certified by this review. The CPU production-body harness uses transport shims and is not the new engine doctest suite. A successful shader compiler run alone does not establish live GPU behavior.
