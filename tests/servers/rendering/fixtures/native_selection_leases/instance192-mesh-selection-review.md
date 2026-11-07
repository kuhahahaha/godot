# Instance192 mesh CPU-selection candidate review

Reviewer `/root/review_vertex`; read-only review of 01-selection-leases.patch and the MeshStorage portion of 02-selection-tests.patch, with adjacent material/shared-shader relationships. No active production/test edits, builds, tests, or Git operations. Candidate base/candidate SHA256 entries in IDENTITY.json all match the files read. This review does not authorize candidate application or imply GPU acceptance.

## P1 — external surface append bypasses the whole-mesh selection lease

Candidate mesh_storage.cpp:580-639 (`mesh_add_external_surface`) is unchanged from base and has no `mesh->selection_holds` guard. The newly guarded ordinary `mesh_add_surface` does not funnel this separate function. Given an ordinary mesh with at least one eligible surface, acquire selection, then call mesh_add_external_surface with an otherwise valid external descriptor. Its existing preconditions allow the selected ordinary mesh: no blend shapes/instances, surface_count below maximum. It validates native backing, allocates a Surface, reallocates the selected mesh's surface directory, appends the external surface, increments surface_count, modifies AABB/cache and notifies dependents. The existing lease remains active and release later succeeds, so no lease validation detects the mutation.

This breaks the stated invariant that whole-mesh selection fixes surface order/count and excludes external surfaces. Even though appended existing indices retain their order, flattening/count and the directory allocation change under the selection, and the retained object now contains a prohibited surface. Add an original-owner selection guard immediately after mesh lookup, before RD resource validation or allocation/side effects. Test with a genuinely valid descriptor and original native buffers, so an unrelated invalid-descriptor rejection cannot hide the omission. Verify surface_count, directory pointer/content, cached material/AABB, lease count and both output parameters remain unchanged; after all leases return the same valid append should succeed. No source fix made in this review.

## P2 — material-selection mutation probe can pass if its guard is removed

02-selection-tests.patch's mesh test invokes mesh_surface_set_material(selected_rid,0,RID999), but afterwards checks only mesh pointer/count and the Surface pointer. Those remain identical even if material assignment improperly succeeds. Capture and assert original->material (and its exposed material-cache selection where populated) around the probe. The candidate does contain the correct early guard; this is a regression-proof gap, not an observed source mutation bypass.

The candidate tests also omit external append entirely and do not exercise the newly guarded ordinary add, blend count/mode, AABB/shadow setters or the four buffer-region mutators. The important region tests should use nonempty valid data and live original RD buffers, and verify original bytes/selection revision state do not change. Empty/invalid input can fail before the new guard and cannot prove hold enforcement. No test expansion performed; root retains the single build/test lane.

## Original owner and mutation inventory

The mesh guards otherwise sit before their relevant side effects: mesh_free before clear/shadow removal/dependency notification/RID free; clear and remove before instance cleanup and surface detachment; ordinary add before allocation; all four region updates before buffer writes and static_capture_invalidated; material assignment before selection/cache mutation; blend count/mode, custom AABB and own shadow change before metadata modification; instance creation before allocating or associating a deformation instance. Existing physical `_mesh_surface_clear` detach-on-free semantics are not repurposed: CPU selection blocks the public original owner free instead of pretending to own physical return.

Acquisition refuses empty/missing mesh, saturation, blend shapes, existing instances, an own shadow mesh, external/retired/skinned surfaces. Instance creation is then blocked while selected, closing that transition. Path-name mutation is intentionally allowed and does not change geometry selection. Incoming shadow_owners and multimesh users can change their own relationships, but the inspected paths do not replace the selected mesh's surface selection. Ordinary renderer version-cache creation/pass fields are not frozen by this CPU lease, consistent with the explicit no-VA/no-pipeline claim.

Inline getters expose the original surface pointer or rebuild material_cache; these are Core internal borrowing interfaces, not independent immutable grants. New integration must not treat a cached raw pointer as authority to bypass original-owner guards. Mesh CPU retention alone also does not retain the surface material RID or shared shader: selection must acquire the corresponding original MaterialStorage lease for every selected material and unwind all successful prefixes if a later acquisition fails.

## Counter, ownership and return reasoning

Mesh acquisition checks the empty destination lease and UINT32_MAX before incrementing, and all per-surface refusals precede the increment. Success adds one whole-mesh count and records exact storage pointer, RD pointer and original RID. Release verifies storage identity, current RD singleton and render thread before owner lookup; absent mesh/count0 fails without decrement. Success subtracts once and clears the lease, so duplicate release cannot subtract again. Noncopyable with user-declared destructor/copy constructor means no implicit movable lease; explicit release is required before destruction. Mesh generation field is unused, but the full RID plus refusal of owner free currently preserves incarnation; it must not be advertised as an independent generation proof.

The test's two successful leases and staged count2→1→0 establish basic pairing and leave physical immutable_holds/version_count unchanged. Overflow, wrong storage/thread/device and failed-acquire preservation remain untested. Core assertions in the lease destructor expose failure to unwind, but are not an automatic cleanup strategy. MeshStorage teardown does not consult selection_holds; production integration must drain all leases before owner/device/storage teardown and before original accepted-root unpin. The candidate README already treats that integration as outstanding; the primitive must not be published as a completed lifecycle contract.

## Thread and shared-shader relationships

Acquire/release explicitly require the actual RD render thread. Mesh selection_holds is a plain uint32_t, and MeshStorage mutators perform no new lock or thread admission. RID_Owner<Mesh,true> protects directory ownership operations, not arbitrary field races after returning Mesh*. Standard RenderingServerDefault mesh mutation wrappers queue to the server thread (server_wrap_mt_common FUNC macros), which supplies the existing serialization model. Async mesh_create_from_surfaces constructs a newly allocated unpublished RID; that does not by itself show a race on an already selected mesh. Direct native calls, callbacks and teardown must still obey original render-thread serialization. No new mutex/atomic should be inferred from RID ownership, and no demonstrated existing selected-mesh worker race is claimed here.

Material acquisition first uses material_get_native_preparation_data: expected generation, 3D material/shader data, serviced dirty flags and no next pass. It increments both material and shared shader selection counts only after checking both for saturation. Material free/replacement and shader free get early refusal guards; shader code/default texture changes route through _shader_native_immutable_busy, which now sees shader selection count even without native physical state. Existing param/priority/next-pass/queued-update gates see the material count through native_immutable_busy. Provided original owner mutations remain serialized, material->shader cannot be replaced during selection and release decrements the same shared shader. Peer material replacement can alter the shader's owner set but need not be blocked: it does not modify the retained selected material or shader code.

The material candidate test uses two leases on one material, not two distinct materials sharing one shader. It verifies count accounting but does not independently prove peer/shared-owner behavior. A directed shared-shader test should select two distinct material RIDs on one shader, release one, demonstrate shader mutation/free still refuse, and permit them only after the last release. Mesh tests cannot substitute for that separate material/shader proof. Physical shader readiness, direct RD resource frees and actual pipeline custody remain outside CPU selection; native preparation must separately validate/retain real resources.

## Disposition

Do not apply wholesale as complete. Fix the external append bypass and strengthen the material-selection assertion, then validate the guarded mutation inventory and original lifecycle integration. No source/test runtime result is asserted by this review.

Read time: 2026-11-07T01:50:56.532149+00:00

- `work/instance192-selection-candidate/01-selection-leases.patch`: `2619ff1080988f8cf6a07b10a126652b115d6b5a687089ce4264fed830eebfd9`
- `work/instance192-selection-candidate/02-selection-tests.patch`: `17f6ac9c22ee98e3d1d09383c3f8d7c3f1cb34edd64a66023e23b0c7b043eb17`
- `work/instance192-selection-candidate/IDENTITY.json`: `3c9424b101ad118559b86d41545979f333c5bc581fbf60778d6fed2cd43ab0b2`


## Applied R2 source re-review

Root applied/formatted the six candidate selection files; this reviewer read the active mesh source/test diff without editing it. Original candidate findings above remain historical; the following supersedes their current disposition.

P1 SOURCE-RESOLVED / RUNTIME-PENDING: active mesh_storage.cpp:600-606 now checks selection_holds immediately after valid mesh lookup and returns ERR_BUSY before capacity/descriptor/native-RD validation or allocation. It preserves out parameters and has no side effects. The added test constructs three real vertices with two distinct live 432-byte vertex buffers, real uint32 index buffer/index array for three indices, triangle primitive, nonzero material RID and generation, and distinct nonzero allocation generations. Both selected mesh and unheld neighbor have default blend_shape_count0 and empty instance lists, and neither approaches maximum surface count. The exact same descriptor is accepted on an independently allocated unheld original mesh, proving this descriptor does not get its negative from an unrelated descriptor validation. The selected call must return specifically ERR_BUSY and leave added_index UINT32_MAX/added_generation0; later checks preserve selected surface_count1 and original surface pointer. Neighbor cleanup frees only its external wrapper; the borrowed buffers remain owner-managed and are explicitly freed by the fixture. No double free of index_array: its own original index-buffer dependency handles it.

The descriptor's material RID999 is accepted by this API because its contract checks only nonnull material here; this is not a material identity/custody test. Validated real buffer sizes/driver IDs/index-array association are checked by the existing RD validation routine; the neighbor positive exercises those same gates. Current tests do not assert all cache/AABB bytes, but the early source guard precludes their modification and count/pointer checks plus exact error distinguish the found append bypass. Same-selected-owner append after last release is not exercised (the positive uses a neighbor), which is a remaining coverage refinement rather than a blocker for the repaired guard.

P2 SOURCE-RESOLVED / RUNTIME-PENDING: the active test captures original->material before refusal probes and checks exact equality afterwards. The ordinary surface helper initializes no material RID, so material_before is empty while the attempted assignment is RID999. Removing the source guard would therefore be detected; pointer/count equality alone no longer masks mutation.

No new mesh blocker found in the applied R2 diff. Counter and thread/lifecycle limits above still apply; this is not acceptance of Instance192 Create integration or any GPU result. Root's unique build lane was active at this read; no build or test was executed by this reviewer.

R2 read time: 2026-11-07T01:59:25.948066+00:00

- `servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp`: `77bf85f81091ff1a1bc1f40822d6fbf817c85f707ffa75ed0403c94b8e4c33e2`
- `servers/rendering/renderer_rd/storage_rd/mesh_storage.h`: `907e64669adac18d5656c6515093dde94bf32c020c565c6411be541f71f0250c`
- `tests/servers/rendering/test_mesh_storage_external.cpp`: `36c275b9db5c6f3ebd0cbec0570d68052396e041719a58886d2e9bb0ec2db7ca`
