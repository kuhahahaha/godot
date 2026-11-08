# Independent review: held material observation

Reviewer: default Sol independent review context; no Astra. Reviewed 2026-11-08 against HEAD `b220d7467b43ac4e921f5fda6944394083b60997` and patch SHA-256 `951eee1b1fc7ee3fdb2290ea0adc92cdfadcb7352a04e6626df323c740407ab0`.

**Disposition: no blocking source-level finding in the requested six-path scope.** Integration may proceed to the original owner's matched native build and actual Metal fixture validation. This is a static independent review, not runtime acceptance.

## Evidence and scope

Read repository `AGENTS.md`, `.agents/skills/godot-fr/SKILL.md`, candidate README/manifest/validation, entire six-path patch, corresponding original and integrated source, original `material_selection_acquire/release/get_shader_features`, native preparation admission and material/shader mutation/free guards. Directly verified all six before/after snapshots against candidate manifest hashes. At review time all six live paths equal the isolated **after** hashes, reflecting root's integration; manifest's historical `live_still_matches_before` fields are packaging-time observations, not the present live status. Only this review file was written by this review task.

## Production correctness and authority

`material_storage.h:147–158` adds four value fields: material RID, generation, shader-version RID and inverse boolean. No raw compiler/backend pointer escapes through this new result; no new lease, reference count, allocation, registry, issuer, ABI binding, resource mutation or renderer admission occurs. The shader-version RID is an equality observation under the existing lease, not independent shader-use permission.

`material_storage.cpp:1232–1245` delegates to the existing `material_selection_get_shader_features`. That original method authenticates owner/device/render thread, original material RID/generation and holds, original shader/data/3D type, serviced uniform/texture state, absence of next pass, compiler result and exact original shader/version coherence. Every non-OK return exits before assignment to caller output. The new value result is fully initialized locally and committed once after success. Therefore every existing error branch leaves all four caller fields unchanged, without duplicating or weakening selection policy.

Original acquire increments material and shader holds only after preparation admission and overflow checks. Original release verifies and decrements those same holds and clears the lease; its noncopyable, crash-on-unreturned-custody type is unchanged. The new observation neither returns nor extends custody. Returned old leases remain unauthorized even while a later lease of the same material is active.

## Actual fixture and oracle consistency

The callback remains parameterless, tests-only and bound to the original main RD render thread outside active command lists. Material cases create actual MaterialStorage shader/material resources, service queued updates, acquire original leases and read actual compiler facts. The seven independent compiler examples retain their own expected inverse facts; alpha-hash and explicit inverse use are distinct from plain/scissor/normal cases.

Static count and label audit agrees across C++, Python and GDScript: the previous non-material 131 checks, one material-owner check, seven cases with 17 labels each, and 19 replacement/mutation/sibling labels total **270 unique expected labels**. Both oracles require exact cardinality and exact expected membership; duplicate labels necessarily fail cardinality/membership (and GDScript explicitly tracks seen labels). Missing branch-dependent checks cannot silently pass. Python additionally requires every row true, empty failures, expected device/backend startup, matching host before/after SHA, process exit zero, one exact PASS, and no error/assertion/crash/leak diagnostics. No permissive production flag or exception suppression was introduced.

The new checks cover sentinel preservation for unissued/returned/invalid selections; original material/generation/version projection; old retained material surviving independent replacement; return of replacement before mutation/free; real original parameter and shader mutation after retained return; stale-generation and dirty admission refusal; old returned lease against a fresh lease; and independent return of two leases for one material. Original `_material_set_shader`, `material_set_param` and `shader_set_code` generation/update mechanics support the expected stale/dirty branches. Replacement shader/material resources are independent; this cannot be reported as execution of Host resource copy-on-write.

## Cleanup and limitations

On the supported successful branches, every acquired lease is explicitly released before its material/shader is freed. Unexpectedly successful empty/stale/dirty admissions have explicit release cleanup. If an expected acquire fails, the fixture records failure and omits dependent checks, which strict oracles reject; no success is inferred. A failed release leaves the original lease active and triggers existing custody/owner guards rather than being converted into success. The projection introduces no C++ exceptions, early allocation cleanup obligation or new background work.

Nonblocking coverage limit: unchanged output on authenticated compiler/dirty/next-pass/device error branches is established by the single assignment-after-success implementation and inherited validation, rather than a separate runtime injection for every branch. The test named `sibling_return_preserves_retained` observes the old material after replacement return/update and before replacement destruction; subsequent old release still validates original custody, but this is not a post-destruction compiler observation. Keep claims at retained-source isolation as candidate README does.

No build, syntax compilation, runner import/execution, native test or GPU test was performed by this reviewer. Python source was AST-parsed only. Candidate syntax/format/TDD compile-red files were inspected as recorded evidence and were not rerun. Root owns actual matched host build and Metal Mobile/Forward+ execution. No full profile qualification, public transport, Host COW, dispatch/source admission, GPU completion or physical-return acceptance is established by this review.
