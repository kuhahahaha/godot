# Independent source review — prospective VA plan

Reviewer: independent Sol agent. Date: 2026-11-08. Live revision: `851b46b8e0ba2ed69ae44a18ab2768cc48457e13`.

Scope: candidate patch and complete after files against live MeshStorage/RenderingDevice, repository AGENTS and Godot FR skill, and `PROSPECTIVE_VA_IMPLEMENTATION_MAP.md`. This reviews the authorized implementation candidate, not a new plan. No production edits, builds, tests, or GPU execution were performed. Existing untracked work was preserved.

## Blocking findings

### P2 — A cached failed ordinary Version is misclassified as creatable

Candidate `after/.../mesh_storage.cpp:869–883` copies the first matching Version but does not distinguish “no matching Version” from “matching Version whose VA/format creation failed.” Ordinary live `mesh_surface_get_vertex_arrays_and_format` (`mesh_storage.h:785–789`) publishes the Version/count before factories run. A failed `vertex_array_create` can therefore leave a real matching entry with a nonzero format and null VA. Future lookup returns that exact entry (`mesh_storage.h:765–775`) and does not retry it. `mesh_surface_hold_derive` consequently returns `ERR_CANT_CREATE` for this state.

The candidate instead returns OK when the directory is unfrozen and descriptors/backing RIDs are valid. Its result can even contain a null `existing_vertex_array` and nonzero `existing_vertex_format`, contradicting the documented missing-Version representation. A caller interpreting this as one FUTURE_VA may pass admission and create other resources before discovering that the actual derive path cannot create that VA.

Precise fix: track whether the key matched separately from VA/format validity. If matched but either ID is invalid, return `ERR_CANT_CREATE` with the already-reset output; do not model it as a missing Version. If absent, leave both IDs null/zero. Preserve external generation matching. Add a focused failed-cached-Version fixture when testing becomes authorized; verify no future factory is attempted.

### P2 — Surface counter and Version-directory saturation are absent from prospective eligibility

Candidate `after/.../mesh_storage.cpp:848–883` checks only that `immutable_holds` is nonzero. Actual `_mesh_surface_hold_acquire` (`live mesh_storage.cpp:864`) rejects `immutable_holds == UINT32_MAX`, including an existing Version. This deterministic Surface-level failure is invisible to the later RD backing checks because the saturated counter belongs to MeshStorage.

For a genuinely missing Version the candidate also returns OK when `version_count == UINT32_MAX` or a next Version allocation exceeds `SIZE_MAX`. The actual external factory explicitly refuses those cases (`live mesh_storage.cpp:681`); the ordinary factory increments its uint32 count without a corresponding guard (`mesh_storage.h:785–787`). A scratch plan must not advertise a future VA in an impossible directory-growth state, and RD cannot check this metadata boundary.

Precise fix: reject Surface immutable-hold saturation before publishing any plan. Under the Version lock, after determining there is no existing matching Version and before returning a future plan, check `s->version_count == UINT32_MAX || uint64_t(s->version_count + 1) > SIZE_MAX / sizeof(Mesh::Surface::Version)` and return `ERR_OUT_OF_MEMORY`. Existing valid Versions need no directory growth, so do not apply the growth rejection to them. Keep real execution revalidation. The generic ordinary factory's pre-existing unchecked publication is a separate existing risk, not a regression introduced by the helper extraction.

## Verified source behavior / nonfindings

- Ordinary extraction preserves descriptor order, full layout computation before mask filtering, compression and 2D positions, packed normal/tangent handling, custom/skin formats, implicit `UINT32_MAX` binding values, motion previous-location duplication, and point-size frequencies. Generic factory effective vertex count remains zero for point-size emulation.
- Buffer extraction preserves ordered buffers, default fallback for missing attributes, tangent default even when present, instance current/previous selection, and binding offsets. The normal binding offset retains the old uint32 multiplication before assignment to uint64; this is source-equivalent, not a newly widened calculation. Normal production surface buffer sizes constrain valid large surfaces; this candidate is not evidence that broader malformed/overflow metadata is safe.
- External extraction emits all ten current descriptors even for mask zero or a subset; validates unsupported bits; preserves binding 0/1, stride144, UV/custom offsets, optional motion descriptors, actual vertex count, and empty-offset-vector semantics (RD supplies zero offsets).
- Ordinary high mask bits remain ignored by descriptor/buffer loops while the full uint64 mask remains the Version key. External rejects bits outside its supported set. Adding a blanket ordinary mask rejection here would change current semantics.
- Source checks correctly reject missing device, wrong RD/thread, null/moved/invalid source, transferred physical custody, returned/returning metadata, and active execution/finalization states. Retained detached COW surfaces remain eligible; no current mesh RID is resolved. Saturation is the exception identified above.
- Version lock ordering matches the documented Surface-before-RD requirement. This function calls no RD/driver resource factory, changes no Version/hold counter, and uses scratch Vectors only. Caller must keep source and recompute/revalidate before execution; returned values do not themselves retain any resource.
- Frozen directory lookup permits an existing valid keyed Version and refuses a missing one. External allocation generations participate in matching.
- Null default/backing RIDs are rejected without inventing resources; eagerly created production defaults are reused. RD ownership/retirement/range validation and complete immutable graph closure are explicitly deferred integration gates, not claimed here. Existing/global defaults must still participate in later closure and lifetime checks.

## Verdict

Needs the two blocking fixes above before this plan is relied upon as the MeshStorage portion of a prospective no-factory rejection gate. No behavioral regression was found in the ordinary/external helper extraction itself. Syntax evidence in the candidate is not runtime or semantic acceptance; tests remain unexecuted.
