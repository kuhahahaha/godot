# Independent implementation review — ready shader lookup

Reviewed commit: `c6366c051f5930c8a989ef1e182c475ee5e67c16` (HEAD at review), all five changed files. Read repository `AGENTS.md` and surrounding initialization, compilation, metadata creation and destruction paths. This was an independent static review; no source edits, build, or test execution were performed. The only written artifact is this report. Existing unrelated untracked work was left intact.

## Verdict

No material production correctness defect found in the reviewed change. The implementation supplies a conservative, side-effect-free readiness observation under the existing render-thread and owner-lifetime discipline. It returns borrowed identities, without retaining them or guaranteeing future availability.

## Production checks

- ShaderRD authenticates the singleton RD's original render thread before reading version state. Invalid variant indices are rejected before indexing; variant/group enablement is checked without enabling either. The version RID resolves through this ShaderRD's original owner, and version state is observed under its original mutex.
- Dirty, invalid, initialize-needed, and any nonzero compilation-task token refuse with `ERR_BUSY`. This deliberately also refuses completed-but-unjoined tokens and tokens in other groups. Unlike `version_get_shader()` / `version_is_valid()`, the new method never calls initialization, compiler start/end, task completion waits, placeholder allocation, cache loading, or group mutation. Acquiring metadata mutexes can wait; “side-effect-free” does not mean lock-free.
- Both variant arrays are bounds checked. A null shader or empty byte entry refuses. Nonempty bytes alone are insufficient: the original RD accessor resolves the shader in its own metadata owner and requires a nonzero native driver identity. `shader_create_placeholder()` has no such identity. Successful container creation installs reflection and driver identity together, so a real zero reflected vertex mask remains valid.
- The RD accessor independently authenticates its own render thread and uses the existing RD metadata lock. Missing metadata returns `ERR_INVALID_DATA`; driverless metadata returns `ERR_UNAVAILABLE`. It performs no driver operation or compilation.
- ShaderRD passes a local mask to RD and copies both caller outputs only after success. RD likewise writes its output only on success. Every inspected refusal preserves caller outputs. The old getter and existing callers are unchanged; this commit adds APIs and tests without integrating a production caller.
- These APIs do not establish a pin, lease, generation, retirement barrier, or independent lifetime guarantee. Existing owner lifetime/serialization rules remain necessary; readiness at observation time is the supported claim.

## Tests and cleanup

The extra friend-access helper is under `TESTS_ENABLED`, using the existing test friend classes. It constructs an original ShaderRD owner/version, then explicitly injects Version fields, arrays, group state and a synthetic nonzero task token. Its comment truthfully describes unit-state injection rather than actual compilation. The native shader metadata comes from the surrounding original RD container-creation path with its mock driver; this remains CPU fixture evidence, not GPU execution.

Covered assertions include invalid version, negative variant, newly dirty version, initialize-needed, pending token, disabled group, missing variant storage, actual RD placeholder with nonempty bytes, successful exact variant with a legitimate zero mask, and invalid version state. Direct RD checks cover missing metadata, driverless placeholder and compiled mock-backed success. Refusal checks preserve sentinels; native-create counts remain unchanged across the helper. Dirty-state/empty-array and pending-token checks additionally detect representative unintended work.

The token is explicitly reset to zero before any version destruction. Injected native shader references are removed from the version arrays before `version_free()`, preserving the outer fixture's shader. The placeholder is removed from the RD metadata owner, the original version is freed through `version_free()`, and the scoped ShaderRD is destroyed before restoring the previous singleton. No synthetic task is joined on the normal/check-failure cleanup path.

In this repository's no-exception doctest mode, `REQUIRE` does **not** abort execution: `tests/test_macros.h` enables `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`, and doctest's no-exception `throwException()` is empty. Thus failed CHECK/REQUIRE assertions do not themselves skip explicit cleanup. One narrow fixture robustness limitation remains: the initial `REQUIRE(version_rid.is_valid())` is not a null guard before dereferencing the owner lookup if version creation ever fails. Here the fresh owner is initialized with two variants and a valid group, so its ordinary precondition failure is excluded; this is not a production readiness defect or a blocker for the observed run. The helper also has no exception-safe singleton restoration for exception-enabled builds if that REQUIRE throws.

## Evidence and remaining coverage limits

Inspected the existing `full-tests-confirmed.log`: 10 cases and 14,001 assertions passed, zero failures, Status SUCCESS. Exit 0 was supplied with the review request; this review did not independently rerun or reconstruct process exit status. This result does not demonstrate a real GPU, real asynchronous completion, or a real partial-cache/fallback-compiler-failure chain.

The added tests do not directly execute foreign-thread/no-singleton refusal, positive out-of-range variants, disabled individual variants, foreign-owner version RIDs, null shader/empty byte entries, unequal array sizes, actual completed-but-unjoined tasks, or ShaderRD propagation of an absent RD metadata RID. Those branches were inspected in source. Nonzero successful masks and multiple successful exact variants are also not demonstrated by this fixture. Existing `REGRESSION_PLAN.md` and `REVIEW_IMPLEMENTATION.md` describe the earlier syntax-only stage and should not be read as a current comprehensive execution report; their full cache-failure-chain scenario remains unexecuted.
