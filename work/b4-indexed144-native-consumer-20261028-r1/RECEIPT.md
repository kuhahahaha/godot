# K05 original Indexed144 native consumer fixed delivery — 2026-10-28

Status: implemented native seam and verified private fixed host; production B4 integration and full branch acceptance remain open. Canonical engine stays SHA882d69abce4d6be4418e5121f48a444c078f40f17c4087ab0971b895eb932500. No SDK/product builder was invoked; client uses frozen SDK archive cb0fbc01daa4f907d148ecaf70a5d29616d23aa01b4ef0939adefd424aec2cec, before/copy/after identical. This mixed-worktree private archive/link evidence is not a clean-checkout/canonical integration claim.

## Implemented source behavior

Both Forward Mobile and Forward+ use a distinct cold Indexed144 shader profile, direct current and previous Float144 position/normal/tangent decoding and handedness. Ordinary variant indices/math remain intact. Actual draw, reflection cache and precompile choose the actual selected surface profile. VA cache keys include input mask, motion, point policy and both allocation generations; candidates publish only after successful original format/VA construction. Real replaced buffer bindings are tested. External append maintains aggregate AABB, skeleton invalidation, multimesh and shadow-owner dependency notices; immutable attributes are advertised. External surfaces use their current surface for shadow paths, reject deformation instances and prohibit ordinary region writes. Original clear frees borrowed VAs/references while preserving the original buffer owners.

Independent review found two neighboring defects and the final source closes both: enabling a new shader group must preserve any dirty version for its original whole-version lazy refresh; ordinary vertex/index region updates must not mutate immutable borrowed buffers (all four region-update entry points reject external surfaces).

## Actual red/green evidence

- Source-bound decoder: actual original helper and both current/previous callsites, dynamic SSBO inputs, actual Metal. Original packed decoder gives8 wrong vec4s per renderer; fixed helpers give8 correct. RED engine0/checker1 → GREEN engine0/checker0. Math evidence only, not real vertex fetch.
- Actual cache receiver: accepted VA_RED rc1,4cases80assertions/9failures. Final expanded original receiver includes format/undersized-VA failure with no published row, exact masks/generations, replaced bindings81/82, point separation and borrowed clear. FINAL_NATIVE_TESTS:26cases555assertions, rc0. Three foreign-camera diagnostics are expected existing negative history cases.
- Actual render fixture: original native MeshStorage add→normal renderer draw/pipeline→SubViewport image. Ordinary ArrayMesh is an independent equivalent input. Valid red before AABB fix:1976 covered/1976 different, rc82. AABB repair:1976 covered/0different, rc0.
- Native contract red: bounds1/attributes0/deformation_rejected0, rc81. Final attributes/bounds/deformation checks all1.
- Shader cold-group red: two separately edited ordinary variants expected2/3 but native compute readback1/1, rc81. Fix readback2/3 after first cold-group enable.
- Borrowed write red: original vertex/index bytes both changed, rc81. Fix both unchanged through original entry points.
- FINAL_RENDER_MOBILE and FINAL_RENDER_FORWARD_PLUS: actual Metal/M3 Pro, expected cold-group2/3, immutable bytes, correct bounds/attributes/rejection, covered1976/different0/max_error0.0, load0/unload0/process0. RGB compared at every96×96 pixel, ordinary alpha coverage >100 and <8000. Identical background cannot pass; output alpha itself is not separately compared.
- FINAL_INPUTS_BEFORE/AFTER:21 file pins identical, independently recomputed by reviewer. FINAL_RUNS records exact commands. FINAL_EXPANDED_ARCHIVE_BUILD/PRIVATE_LINK rc0. Final diff check0.

Initial fixture run NATIVE_RENDER_RED.rc0 is INVALID acceptance: wrong ordinary CUSTOM0 Variant prevented construction and RGB coverage counted background. INITIAL_FIXTURE_INVALID.md documents correction. INITIAL_VA_RED_FIXTURE_CLEANUP is a test teardown fault, not behavioral terminal. VA_GREEN and earlier4/80 runs had fake-buffer dependency-map diagnostics; FINAL_NATIVE_TESTS fixes metadata teardown and supersedes them. Original raw logs remain unchanged for audit.

## Scope and remaining work

This is a private original native seam, not a new production actor/API/owner. Helper/client/host retain exact native/SDK lifecycle; generic buffer frees and cleanup/unload markers prove requested cleanup only, not E05 physical retirement or OS unmapping. Descriptor generations are private fixture labels, never owner authorization.

No production ANGE B4 caller or six K04 particles_external_* ports were found. D still owes K04 core output implementation and full native branch acceptance; it is not marked externally blocked or complete. Original E09 use ref and retained E05 current/previous/index sources plus genuine K04 packet selection must reach this same storage/renderer; B owns that real request/record join and C04 retains typed source authority. No fallback permission is added.

Unproved here: real per-view motion/strict-zero invalid history/shadow-depth/mirrored-host combinations, CUSTOM1–3 material outputs, broad renderer concurrency, original qualified source lifetime/physical return, production E09/U08/Ready/G1. Existing source-bound previous decoder math is narrower than actual motion acceptance. Next adopted priority is original E06 core controlled-build stamp producer/get_version_info projection, independently executable while production joins and E20 adoption proceed. Long goal remains active.
