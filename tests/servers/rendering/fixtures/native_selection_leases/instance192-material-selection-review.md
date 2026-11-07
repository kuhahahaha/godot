# Material/shader selection lease candidate — independent review

2026-11-07. Reviewed MaterialStorage/shader parts of `01-selection-leases.patch`, corresponding material tests in `02-selection-tests.patch`, candidate source copies and original RenderingServer queue wrappers. No candidate/source edits, build, execution or Git. This is a static review of an unapplied candidate, not an acceptance receipt. Scope does not validate the mesh half or Instance192 Create integration.

## Result

No new confirmed material/shader identity or counter defect found in this candidate under the existing original render-thread mutation contract. The selection primitive is internally coherent, but the proposed tests do not establish queued mutations, shared shader across different materials, dirty replay, overflow, or wrong-owner/thread return. Those are concrete remaining verification items before relying on it in a generation Create transaction.

## Original identity and counters

- `material_selection_acquire` checks original RD render-thread identity, rejects occupied lease, and calls original `material_get_native_preparation_data` with exact expected generation. That rejects missing/non-3D/data-less material or shader, UINT64_MAX generation, dirty input and next-pass. It does not confer native shader/pipeline readiness.
- Both material and shared-shader counters are checked against UINT32_MAX before either increment. There is no half-increment overflow path. Failure before this point leaves counters and empty lease unchanged. A stale expected generation rejects before increment.
- Lease records this storage, RD, original RID and material generation. Return authenticates storage/RD/render-thread, resolves same RID, verifies same generation and nonzero counters, then decrements both and clears the lease. Duplicate return rejects. No RID recreation or replacement occurs on return.
- Lease does not separately save shader RID/pointer. In the reviewed mutation inventory, that is protected by the retained material: material free and `_material_set_shader` reject before any generation increment, data deletion, old owners erase or shader replacement. Shared shader free/code/default-texture mutation are also blocked. Thus the current original shader pointer is stable within this contract; integration must not mutate these private fields or destroy MaterialStorage behind a lease.
- Counter fields are ordinary uint32_t, not atomics. This uses original RT serialization, not a new cross-thread ownership protocol. `shader_create_from_code` / `material_create_from_shader` may construct new resources asynchronously, but selected existing objects' ordinary setter wrappers enqueue or execute on server thread. No thread-safety/TSan result is established here.

## Queued mutation, free and shared shader

- Normal public `shader_set_code`, default-texture setter, material setters use RenderingServerDefault command wrappers (`FUNC2`, `FUNC3`, `FUNC4`): enqueue off-thread; direct server-thread path flushes pending original commands before executing. Candidate guards execute at actual setter execution time. A queued shader edit serviced while selection remains active is refused, rather than silently applied later by the selection primitive. Tests should assert this actual queue behavior explicitly; candidate adds no retained mutation retry system.
- `_shader_native_immutable_busy` checks `shader->selection_holds` before relying on ShaderData/native version readiness. Consequently CPU selection blocks code/default-texture mutation even before any native physical hold exists.
- `shader_free` checks shared shader selection count before locking and before its owners-detach loop. This prevents a selected owner's `_material_set_shader` refusal from leaving `shader_free` spinning over that same owner, and preserves actual ShaderData/mutex/RID.
- A second material on the same shader contributes to the shader counter independently. Freeing or changing an unselected sibling erases only that sibling from shader owners; it need not invalidate the selected material. Returning one selected material must not permit shader mutation until every selected sibling returns. Existing new tests use two leases on one material, so this is source reasoning, not demonstrated multi-material behavior.
- `shader_set_path_hint` remains mutable. It changes diagnostic path metadata and forwards a hint, without replacing shader data/code/generation in this implementation; it is outside the frozen selection inputs described by the candidate.

## Dirty queue behavior

- MaterialData `native_immutable_busy` adds material selection count to existing native-resource/hold checks. Acquisition requires nonnull data and a serviced clean material, so existing param/priority/next-pass guards see the selection.
- `_update_queued_materials` moves the current queue to a local finite copy, removes each entry, and, if selected, requeues it once into the original queue with false/false flags while preserving its existing dirty flags. It does not spin on a freshly requeued entry within that local pass or call `update_parameters` for the selected material.
- Original dependency erasure/global-uniform paths can queue a selected material. They do not bypass the update refusal: dirty work remains pending until release. Another acquisition sees dirty flags and returns ERR_BUSY. Returning the existing lease does not require clean flags and therefore is not trapped by that deferred work.
- This does not retain textures, RD descriptor sets or buffers. Their original mutation/return may still invalidate physical preparation, which must be checked by the later actual physical hold/preparation path. CPU selection is not a physical completion or immutable-resource grant.

## What the added tests actually demonstrate

The material patch extends the original CPU controlled native material fixture after its original `_update_queued_materials` serviced set creation. It checks stale expected generation, duplicate lease, two leases on one material, unchanged physical hold count, blocked material/shader free and several setters, stable original owner pointers/generation/shader, staged release counts and duplicate-release refusal. It then resumes the existing physical-hold test. Expected error suppression is scoped to intentional refusal probes.

Missing directed cases:

1. Two distinct materials sharing one shader: acquire both, release one, prove shader_free/code/default mutation still refuse; change/free an unselected sibling safely; release last, prove allowed mutations resume.
2. Original command queue: enqueue edits before/after selection using the real queue, inspect code/default texture/generation/owner identities and eventual refusal/success. Existing direct setter tests do not establish FIFO behavior.
3. Original dirty queue: enqueue selected material via existing dependency/global path, run repeated finite queue updates, prove no parameter update or dirty clearing and exactly one queued entry; return lease, run next update, prove work serviced. Also acquire against preexisting dirty input must reject with no counter increment.
4. Overflow each counter separately with exact undo confined to test metadata: material UINT32_MAX and shader UINT32_MAX each reject without changing either counter/lease. Include near-limit paired return to prove no wrap.
5. Wrong storage, wrong RD and wrong thread release; rejection must leave both counters and the original lease intact, followed by successful original return. Stale/zero/MAX generation rejection and occupied-lease failure should also prove no changes to the existing live lease.
6. Failed generation Create prefixes, explicit placement destruction and original auto-drain: all selection leases return before owner/slab destruction and no active lease outlives MaterialStorage/RD. Candidate destructor crash is a fail-closed assertion, not an automatic drain implementation.

Do not present the direct original-fixture test as public Instance192 Create, rendered output, GPU completion or successful native shader preparation.
