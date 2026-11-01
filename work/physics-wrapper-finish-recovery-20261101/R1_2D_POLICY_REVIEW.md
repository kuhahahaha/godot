# R1 2D mirror policy review

2026-11-01. Reviewer: `r1_protocol_review`. Read-only review of original 2D wrapper, the adopted `docs/contracts/native-physics-cleanup/2d/FR_PHYSICS_2D_CLEANUP_PLAN.md`, its `RID_ARGUMENT_POLICY_2D.json`, and the eight-case `test_wrapper_stop_2d.cpp`. No production edit, builder, compiler or runtime execution was performed by this reviewer. Parent owns 2D RED/GREEN receipts. The reported 3D runs do not qualify 2D automatically.

Input pins at final reread:

- Original `servers/physics_2d/physics_server_2d_wrap_mt.h`: `05e31ed57102ecadda7f657e913d178a8d19a78a605441a19b4d541e83f51c73`.
- `test_wrapper_stop_2d.cpp`: `d9da973a6f29846a193f4d9a151bde0ccfd7f3ec7956091c5ff1cd55687a08d8` (includes the author's added body_get_param INT-zero assertions following this review's finding).
- Normative `RID_ARGUMENT_POLICY_2D.json`: `ca50bcc250307f3013e081b227a31e973ac45c4adda124467b163fbc20309cd2`.

## Must differ from a mechanical 3D copy

### Exact return defaults

The 2D PLAN explicitly selects each RID inventory row's default for stopped wrapper calls. Joining the actual generated-return declarations to those rows found these exceptions to a generic value constructor:

| 2D method | Exact stopped result | Required handling |
| --- | --- | --- |
| shape_get_type | SHAPE_CUSTOM | Named 2D enumerator; its numeric position differs from 3D |
| joint_get_type | JOINT_TYPE_PIN | Named 2D enumerator |
| joint_is_disabled_collisions_between_bodies | true | Explicit bool exception |
| area_get_shape_count | -1 | Explicit int exception |
| body_get_shape_count | -1 | Explicit int exception |
| body_get_max_contacts_reported | -1 | Explicit int exception |
| joint_get_param | real_t(-1) | Explicit return default in the two-argument const-return family |
| body_get_param | **Variant containing integer 0** | Explicit Variant exception; empty Variant is incorrect |

The last row was missing from the initial eight-case fixture. It is not merely a scalar zero: the declaration returns Variant, the inventory says `0`, and current `GodotPhysicsServer2D::body_get_param` at lines 755–757 returns `ERR_FAIL_NULL_V(body, 0)`. The implicit conversion constructs `Variant::INT`, whereas `Variant()` is NIL. The author added a backend nonzero sentinel and assertions for both INT type and numeric zero during this review. The mirror implementation must likewise use the explicit integer-Variant default.

Other generated defaults match their audited constructors/scalars: BodyMode is BODY_MODE_STATIC, CCDMode is CCD_MODE_DISABLED, other booleans false, numeric returns zero, RID/ObjectID empty, Vector2 zero, Transform2D default identity, and the other Variant getters NIL. Do not zero raw Transform2D storage. No further non-constructor generated return exception was found in the finite 2D inventory.

### Compiled surface and custom calls

The original wrapper has **123 generated declarations including free_rid**, **9 compiled custom non-lifecycle methods**, and **6 lifecycle methods**. Keep free_rid explicit in the mirror. This is a wrapper census, not the backend inventory's 117-method/133-position census.

Generated arities are FUNC1/2/3/4/5/6, FUNC1RC/2RC, FUNC2S and FUNCRID. The 2D-only FUNC6 is `joint_make_groove`; 3D's FUNC7 and FUNC3RC are unnecessary here. There are 12 direct creators: eight shape families plus space, area, body and joint. There is no 2D SoftBody, heightmap or custom_shape_create entrance to invent.

The nine custom entries are shape_collide, space_get_direct_state, space_get_contacts, **space_get_contact_count**, body_collide_shape, body_test_motion, body_get_direct_state, is_flushing_queries and get_process_info. The actual contact-count declaration remains covered even though the PLAN's prose custom list abbreviates it.

- `shape_collide` is a compiled nine-argument API in 2D; its 3D counterpart is disabled under `#if 0`. Add a real root scope before backend access. On stop return false without changing either results buffer or referenced count. Preserve the existing Main-thread check.
- `body_collide_shape` is an eight-argument custom API. On stop return false with buffer/count untouched. It has **no existing Main-thread check**. The ordinary foreign-caller fixture correctly catches accidentally adding one or routing a previously direct call into an unpumped foreign synchronous queue. Its recording backend test proves wrapper forwarding only, not arbitrary real-solver cross-thread safety.
- There are six existing custom Main-thread checks: shape_collide, space_get_direct_state, space_get_contacts, space_get_contact_count, body_test_motion and body_get_direct_state. Do not add one to body_collide_shape or get_process_info as an incidental mirror change.
- Preserve `body_test_motion(..., MotionResult *r_result = nullptr)` and leave MotionResult unchanged on stop. Direct-state pointers return nullptr, contacts empty Vector<Vector2>, contact count/process info zero, and is_flushing_queries false after stop.

2D also has canvas-instance-ID generated methods and six backend area-space aliases rather than 3D's four. The two additional aliases are area_attach_canvas_instance_id and area_get_canvas_instance_id. This is an inventory difference, not authorization to implement backend alias/retirement changes in the R1 wrapper patch. Preserve forwarding and stop admission; do not copy a 3D four-alias policy into later 2D backend work.

## Concrete test coverage and remaining gaps

The current eight cases already exercise the four -1 results, named enum/true defaults, both extra collide custom methods with two sentinel buffer entries and a sentinel count, other custom stopped returns, normal inline/threaded lifecycle, inline owner/query finish, preflush payload cleanup and the ordinary foreign body_collide_shape policy. The added body_get_param type/value assertions close the newly identified default gap.

Small, directly relevant missing checks:

1. **FUNC2S output preservation:** seed `List<RID>` for body_get_collision_exceptions, stop, call it and assert the exact list remains. Override the recording backend method to increment calls and append a distinct sentinel so accidental dispatch cannot pass because the Dummy implementation is inert.
2. **Threaded stopped defaults/custom bypasses:** the broad stopped-default/output test currently uses `create_thread=false` only. Reuse its assertions with a fully initialized then finished threaded wrapper to prove no stopped-pump wait or custom backend access. This is especially relevant to FUNC2S.
3. **New arity and 2D constructor/enum representatives:** record joint_make_groove (FUNC6) once on an ordinary accepted path and verify it is inert after stop. Add BODY_MODE_STATIC, CCD_MODE_DISABLED, a canvas ObjectID and Transform2D identity representatives if asserting exact whole-surface default coverage; currently that larger claim would be unsupported.
4. The eight cases do not reproduce the 3D gated barrier/cleanup/late-publisher/lifecycle-admission schedules, or establish independent 2D/3D control counters under a mixed-dimensional callback. Do not label them complete 2D R1 coverage.

Stopped buffer tests currently use Main and do verify no backend calls through the aggregate counter. The foreign body_collide_shape test joins its producer before reading ordinary output fields, so its cross-thread result reads are ordered; no additional definite fixture data race was found in this limited policy review.

## Mirror boundary

Reuse the reviewed R1 control algorithm with separate 2D wrapper state/TLS type and local P2D macro names; preserve pump finish affinity, publisher admission and release sealing. No scheduler redesign follows from this review. WorkerThreadPool's pump-task capacity behavior was already resolved by the parent; this note introduces no small-pool defect claim.

This is a finite default/custom-surface review and implementation checklist. It does not qualify backend native retirement, scene teardown, all RID positions, real GodotPhysics2D solver behavior, or unexecuted tests.
