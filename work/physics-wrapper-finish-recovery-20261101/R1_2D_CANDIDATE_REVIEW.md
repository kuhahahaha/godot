# R1 2D candidate source review

2026-11-01. Reviewer: `r1_protocol_review`. Read-only review of the actual 2D candidate and its gated fixture. No production edit, compiler, builder or runtime execution was performed. Parent reports actual original-2D RED: eight cases, three controls passed, five target cases failed, 38 failed assertions. That is not GREEN evidence for this candidate.

## Reviewed pins

| File below `work/physics-wrapper-finish-recovery-20261101/` | SHA-256 |
| --- | --- |
| `2d_candidate/servers/physics_2d/physics_server_2d_wrap_mt.cpp` | `cdb0bf2db2b772686508fd2b76ddd6b104f9172dd9e952c1afec87acde11d601` |
| `2d_candidate/servers/physics_2d/physics_server_2d_wrap_mt.h` | `59c792f28f8e63c4097fd9ecc055294c45f8db8b447f718d9e5ed63e717559b6` |
| `test_wrapper_r1_gates_2d.cpp` | `c0fc70ff6c1c3d3d8f8cddab837dc6aacdf9e3e5dc98ddf38d9906223c7f16f1` |

Comparison source was current main-tree `servers/physics_3d/physics_server_3d_wrap_mt.cpp`, SHA-256 `58ee7a623badabd3b6b0beb2ae6be116eb5c2db143a2062e67502ed1cdafe96f`. Policy authority is the adopted 2D PLAN and `RID_ARGUMENT_POLICY_2D.json`, as recorded in R1_2D_POLICY_REVIEW.md.

## Result

No definite policy, signature, scope or scheduling **mirror error** was found in these bytes. This limited source conclusion permits the parent's planned candidate build/runtime verification; it does not mark complete 2D R1, backend retirement or scene cleanup accepted.

The following were checked against actual source, including mechanical comparisons of declarations/defaults rather than relying on the generator's claim:

- Original 123 generated entries become 122 generated entries plus explicit free_rid. The declaration comparison found no missing/extra generated method or altered return/argument signature. This includes FUNC6 joint_make_groove, the canvas-ID entries, the const bool parameter and the lone FUNC2S body_get_collision_exceptions.
- All **39** generated const-return defaults match the 2D inventory literals. The four -1 defaults are present. body_get_param uses literal `0` in its Variant-returning override, preserving the same integer-Variant conversion as the existing backend; it does not return NIL. ShapeType/JointType/BodyMode/CCDMode names, the true collision-disable result and constructor defaults match the selected rows.
- All **nine** custom signatures match the original header. Each begins with a root Scope before backend access. The six existing Main-thread checks remain; body_collide_shape has no added Main restriction. shape_collide/body_collide_shape refuse before output buffer/count writes, and body_test_motion preserves its default nullptr result pointer and stopped output. Contact count is present despite the shorter prose inventory list.
- Generated scopes live in the override, outside typed dispatch helpers. Direct helper scopes span preflush and backend call; outer roots span returned-value materialization/helper cleanup. Creator paths remain directly admitted. free_rid keeps release-only admission/sealing, and is_flushing_queries reads wrapper query_active under the mutex.
- After replacing only dimension names (`3D`/`3d` with `2D`/`2d`), the **entire cpp equals the current reviewed 3D cpp**. Thus the mirror retains barrier-before-PARK, whole-flush acknowledgement, RUN borrowed-worker handling, non-cooperative parked polling, task-aware TLS ancestry, rejected-admission exit service, the post-end_sync drain, release sealing and pump-only threaded finish/join ownership. No scheduler modification was introduced.
- Control fields and Scope/TLS belong to PhysicsServer2DWrapMT. They are not shared with PhysicsServer3DWrapMT; wrapper identity is still checked within the 2D TLS chain.

## Gated fixture comparison

The 2D gated file is identical to the final reviewed 3D fixture after the expected dimension, namespace, registration, environment-variable and marker substitutions. The final `negative_hold` fixes are retained. Positive-case gate sequencing and shared-data ordering therefore have no newly introduced 2D-specific discrepancy.

Concrete registration/control identities were checked:

- File stem and force-link token are both `test_wrapper_r1_gates_2d`.
- The access friend is `TestPhysicsWrapper2DR1Access`; its name matches the candidate header.
- Namespace is `TestPhysicsWrapper2DR1Gates`; test selector prefix is `[PhysicsWrapper2DR1Gates]`.
- Compilation guard is THREADS_ENABLED and not PHYSICS_2D_DISABLED; both server includes target 2D.
- Environment variable is `GODOT_TEST_PHYSICS_WRAPPER_2D_R3_CONTROL`, with exact `early-park` / `early-ack` values.
- Negative markers are `PHYSICS2D_R3_PARK_BEFORE_BARRIER_PRE_WAIT` and `PHYSICS2D_R3_ACK_BEFORE_CLEANUP_RETURN`, exit 86. Fixture gate failures use PHYSICS2D markers and exit 87, which remains failure rather than RED evidence.

The guarded Main negative paths cannot reach the hazardous barrier/backend continuation before the independent observer traps. Final checks still precede redundant cleanup finish where that ordering is necessary to avoid concealing the lifecycle boundary under test.

## Remaining evidence boundary

Parent must run the eight 2D starter cases, four gated positives and both separately selected negative controls on matching candidate/binary bytes. Source similarity to a 3D GREEN run does not supply those 2D results. Existing missing coverage described in R1_2D_POLICY_REVIEW.md remains separate: seeded collision-exception output-list preservation, threaded whole-surface stopped defaults, explicit accepted FUNC6 argument forwarding, and independent mixed-dimensional operation. The gated cases exercise a recording backend and do not establish native GodotPhysics2D resource retirement or scene continuation correctness.
