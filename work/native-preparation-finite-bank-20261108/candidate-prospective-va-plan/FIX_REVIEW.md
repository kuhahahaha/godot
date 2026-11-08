# Independent fix re-review

Reviewer: independent Sol agent. Date: 2026-11-08. Scope: live diff against `851b46b8e0ba2ed69ae44a18ab2768cc48457e13`, specifically the prospective plan fixes and new test fixture safety changes. No production/test edits, builds, or tests performed by this reviewer.

## Disposition

Both production findings in INDEPENDENT_REVIEW.md are resolved by source inspection. Both fixture findings in TEST_REVIEW.md are resolved. No remaining blocking finding was identified within this MeshStorage value-plan slice.

- A separate `matched_version` flag now records an exact ordinary key or an external generation-qualified key. A matching entry with null VA or zero format returns `ERR_CANT_CREATE` while `out` remains fully reset. It cannot become a symbolic future VA. Valid existing entries retain their original IDs; genuinely absent entries retain null/zero IDs.
- `immutable_holds == UINT32_MAX` now returns `ERR_INVALID_PARAMETER`, consistent with `_mesh_surface_hold_acquire`. This check applies to both existing and missing Versions because either eventual derived hold would increment that Surface counter.
- Missing-Version growth checks execute under the Surface Version lock and only when no matching Version was found. `version_count == UINT32_MAX` short-circuits evaluation before the uint32 `version_count + 1` expression can wrap. For smaller counts, conversion to uint64 before comparing with `SIZE_MAX / sizeof(Version)` safely detects allocation multiplication overflow on supported size_t widths. A valid existing Version is not incorrectly rejected merely because directory growth would be impossible. Frozen missing lookup still returns unavailable before growth consideration.
- The lookup still scans the actual Version directory before its missing-growth check; this is correct for legitimate directory metadata and does not justify constructing a synthetic huge count backed by a one-element array. The omitted UINT32_MAX Version-count fixture is therefore appropriate. Growth rejection is source-reviewed, not runtime-proven by these cases.
- All plan Vector indexing in prospective_plan now has a size/nonempty guard. Nonfatal CHECK failures can proceed to cleanup without introducing exception-dependent REQUIRE exits. Independent size/count assertions remain in place.
- External borrowed current/previous/index handles are captured before initial acquire and freed once after either branch. This fixes acquisition-failure cleanup without double freeing the passing path. Temporary default/counter/failed-Version metadata is restored before custody release.

## Limits

This is approval of the reviewed source fixes, not completion of the full prospective native admission feature. The previously recorded deferred RD validation, full prospective graph accounting, caller integration, exact execution revalidation, and runtime coverage gates remain. The broader helper-equivalence and fixture coverage limits in TEST_REVIEW.md remain unchanged. Root reported targeted red execution and a green build in progress; this reviewer did not execute or independently observe those runs and makes no public API or GPU acceptance claim.
