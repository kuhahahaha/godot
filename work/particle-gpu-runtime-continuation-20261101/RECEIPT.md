# Particle main-Metal continuation receipt

2026-11-01. Work-only candidate delivered for parent review and the sole controlled builder. No production or existing test file was written by this lane, and no scons/link/main-Metal positive run was started.

- `CANDIDATE.patch`: SHA256 `10864678db85a9a6c51bc57f3bd030d52b875c5cc290b24ba14f899d78340e7a`, 11 files. `git apply --check` rc0 against pinned post-integration source; `base_drift=[]`. Apply only this patch serially; do not overwrite RD files from the candidate mirror.
- `BUNDLE.json` and `SOURCE_BASE.json`: source HEAD `5db6496913f87d118f9f873e7c21941cdbf6ea69`, exact per-file base and candidate hashes. `base/` contains the exact existing-source patch base. The `.compat.inc` candidate is an unchanged quote-include syntax helper, not a changed/adoptable file.
- `SYNTAX.json`: actual clang++ fsyntax-only, final fixture and register cpp rc0; candidate RD module-only, modules-off, and both-module configurations rc0. This establishes syntax, not link, SCons discovery, GPU execution or physical cleanup.
- [Precise old-host entry control](results/20261101-121220/RESULTS.json): immutable FORMATTED host SHA `1d20d3073b1284134b9ce4f16f4fa5ba8301a5f8baee5685eeebbd7e67228f2c`, headless capability-only invocation, exact process exit41 and unique missing-module/gpu_pass=false result. No script or engine error, no timeout, binary/meta and script hashes unchanged. This deliberately does not count as particle GPU evidence.

The next actual main-Metal gate is explicit and still UNRUN. Parent enables `module_particles_runtime_tests_enabled=yes` on a controlled tests=yes/3D editor build, pins the resulting executable and its actual controlled_build JSON, then runs:

```sh
python3 work/particle-gpu-runtime-continuation-20261101/run.py --pin <module-enabled-PINNED_HOST.json>
```

That command uses the ordinary main Metal/mobile renderer with safe render-thread ownership. It must complete 3 original upload-boundary failure/retry sequences, 4 original success-prefix/progress suffix cases, 1 original trail fallback resource failure/retry, six actual async readbacks, original owner release followed by physical driver-return acknowledgement, and no exit leak. The 11 expected injected engine error lines are counted exactly; extra engine errors fail. Source extraction, dummy, a missing class, dispatch count alone, CPU command sync and RID disappearance cannot pass this gate.

The module is default off and tests-only. It reuses the single existing server registry implementation and does not depend on or silently expand the bounded Skinned capture module. Existing capture observations and hook implementation remain capture-only. New skip injection and fixture friendship are also module gated. No public production API or qualification/provider authority is added.

[DESIGN.md](DESIGN.md) contains the source chain, organization choice, precise branch matrix, lifecycle, build scope and remaining independent review/link/Metal gates. This receipt is fixture implementation delivery, not completed particle GPU acceptance or ANGE product/owner acceptance.

## Actual main-Metal gate follow-up — 2026-11-01

The previously listed gate was subsequently executed on the pinned module-enabled host. The fixed runner returned rc 0 on Apple M3 Pro (Metal 4.0, Mobile renderer, safe render thread), with 292 native checks and 8 outer checks true and no failures. The executable SHA-256 remained `bb3508c7b4e1164e86d6f2c192ab4e15a3d0559f4fab9643652b99e14dad8523`; controlled-build metadata SHA-256 remained `a1a1183ad91177c8520cd03430dc21032f204e47ec12cb93b77fea397bf301ed`.

The run observed six queued and six completed async readbacks, four rendered frames, 26 original buffer RID releases followed by physical return acknowledgement, 22 native shader/pipeline/uniform resources freed, and no exit leaks. The exact 11 injected engine diagnostics matched the expected counter; no extra errors or script failures occurred. The full raw result is `results/20261101-122958/RESULTS.json` with review in `METAL_GREEN_REVIEW.md`.

This closes the package's tests-only actual main-Metal retry, progress, trail-resource, readback, and physical-retirement gate. It remains separate from a production release build and from ANGE owner, typed-close, Ready, or G1 acceptance; the historical pre-run handoff above is retained rather than rewritten.

## Current dirty-source rerun — 2026-11-02

A fresh `scons platform=macos target=editor tests=yes module_particles_runtime_tests_enabled=yes -j8` completed with rc 0 and `source_drift=[]`; the resulting editor was pinned separately under `bin/godot-particles-runtime-current.arm64` with SHA-256 `19f8c9f542fb64db713a03a31413c8df9622100f50d81969254195f9527760e4` and controlled-build metadata SHA-256 `3ef8b748c7777311d20168050828c33939f35ca3897e7c29d5076a83cfe68df2`.

The subsequent real Metal runner invocation is recorded in `results/20261102-191118/RESULTS.json`. It returned rc 41 with the explicit `tests_only_module_missing` result. The source has `modules/particles_runtime_tests/config.py:is_enabled() == False` by default and an opt-in `servers/rendering/SCsub` integration. The resulting executable nevertheless reported the explicit missing-module capability result under the pinned runner; the build artifact and generated module-registration state require a follow-up inspection before any positive GPU claim. This is a valid capability-negative result and is not GPU evidence; the older 2026-11-01 green host remains pinned to its original source window and is not attributed to the current dirty tree.

The candidate module wiring and fixture expansion remain staged as review material in `CANDIDATE.patch`; they were not applied to the mixed dirty production tree in this rerun.

## Current explicit-module rebuild and Metal rerun — 2026-11-02

The prior capability-negative run was traced to an executable that did not contain the opt-in module. A fresh controlled build with `scons platform=macos target=editor tests=yes module_particles_runtime_tests_enabled=yes -j8` completed with rc 0. The generated `modules/modules_enabled.gen.h` contained `MODULE_PARTICLES_RUNTIME_TESTS_ENABLED`, and the pinned executable contained `ParticlesRuntimeTestFixture` symbols.

Pinned executable: `bin/godot-particles-runtime-enabled-20261102.arm64`, SHA-256 `703dec7a66e99bf85e336cd3fb368e12f2a146f5aff318d0afa6c211a1406f65`. The controlled-build metadata SHA-256 is `762cfe40a6094141a321b1aa29602b4c43c7c4e8ce27a23ada6bc2b474543510`.

The real Metal/mobile runner completed with rc 0. Receipt: `results/20261102-193655/RESULTS.json`. All checks passed: exact injected diagnostics, six queued/completed readbacks, allocation/resource ownership observations, no script errors, no exit leaks, and `actual_metal_positive=true`. This remains tests-only evidence and does not claim production, ANGE owner, typed-close, Ready, or G1 acceptance.
