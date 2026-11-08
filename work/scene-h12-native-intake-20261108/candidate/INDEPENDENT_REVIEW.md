# Independent review: H12 narrow fixture adaptation

2026-11-08. Reviewed the frozen six-file candidate, complete patch, local SceneCull/ParticlesStorage owner and snapshot paths, build gating, and candidate evidence descriptions. Coordinated with the implementer to confirm the candidate path and frozen source status. No live source edit, build, host execution or GPU run was performed by the reviewer; this report is the only reviewer-written artifact.

## Result

No concrete blocking defect found in the requested narrow adaptation. The candidate preserves the local snapshot boundary, limits the frame-loop change to the new H12 entry, and preserves the old sampling and cleanup implementation. Local numeric/GPU acceptance remains pending.

## Original-owner instance join

The conditional SceneCull friend is confined to TESTS_ENABLED && MODULE_PARTICLES_RUNTIME_TESTS_ENABLED. The module's existing can_build also requires tests and 3D. No production snapshot field or public resolver is added.

The fixture first obtains the receiver's unique real pair snapshot. It then looks up only its own selection_field in SceneCull's original instance_owner, checks the particles-collision base type, exact collision base RID and nonnull base_data, and obtains the real InstanceParticlesCollisionData::instance. The existing ParticlesStorage getter must succeed and match source_key, shape_revision, transform, extents and resolution before second_instance is assigned. This is consistent with the local snapshot implementation and the getter's active/heightfield checks. It neither treats source_key as a RID nor searches another owner or source.

The two framebuffer and sampled texture identities must be distinct. Each target is seeded independently; the direct control and SceneCull path then supply independent observations. The ordinary receiver has no mesh draw pass. The particle exclusion case supplies actual particle draw data for the positive direct control rather than relying on an empty particle source.

## Scheduling, readback and cleanup

GDScript enables the normal render loop only when ANGE_HEIGHTFIELD_SOURCE_SELECTION is nonempty and selects the H12 entry in that same branch. All existing mode dispatch and bindings retain their previous order and behavior when that variable is absent; the original H12 runner removes inherited ANGE_HEIGHTFIELD_* values before setting its exact case. Invalid H12 names exit as failures. The source-based frame-progress rationale matches local Main's draw decision and does not imply a completed local GPU run.

Independently compared the old compute/SSBO/async-readback block with sample_depth_pair: it is a byte-identical move, including its original error returns. Existing callers reach that same block through the added helper call. The original sample_done branch is preserved after the H12 early branch.

The full old cleanup suffix is also byte-identical. Only H12 enters the new prefix, which returns sampling resources, frees the source/receiver/field SceneCull instances and their scenario, then returns direct geometry and the ordinary receiver particle before the old base-resource cleanup. Local SceneCull frees its collision instance through the original owner when freeing the field; clearing second_instance afterwards prevents the old suffix from freeing it again. Both successful async completion and synchronous setup errors use this cleanup path. Non-H12 modes bypass the prefix.

## Evidence boundary and integrity

The candidate README and appended fixture documentation distinguish received owner Metal/M5 evidence from this local adaptation. Recorded C++ VFS and GDScript check-only success are described as syntax evidence, with no object/link or H12 GPU execution claim. The initial missing-field syntax failure is retained. The old intake receipt explicitly describes its earlier receipt-only stage; candidate documentation clearly identifies adaptation and execution as separate stages.

Independently recalculated the five before hashes and all six after hashes, confirmed every existing live source still matches before and the new runner remains absent live, and confirmed observed HEAD matches 35973a577b2f11a9cf51bd1f377b7e375db9df6d. source.patch exactly equals the full frozen delta and its SHA-256 matches d2158a436fe1ae982e13d265a1056b8f59013882f4d25b7d255b8915dbf7ea48. The new runner is byte-identical to the retained original f9ab5fc fixture runner (SHA-256 70e85a95353752229c98e4ac07c22f2190407d7ab5140da9534cc84fdfc9b2b8).

No HeightFrame Current/COW, managed membership, D geometry custody, U05, K01, P1, E34 or Finalize completion follows from this review. If local execution is required, the exact original eight-case runner on a matching host remains the acceptance check.
