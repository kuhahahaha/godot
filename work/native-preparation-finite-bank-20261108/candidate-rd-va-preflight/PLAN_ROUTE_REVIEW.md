# Independent review: original plan to RD preflight route

Default Sol independent review, no Astra. 2026-11-08. Reviewed live six-file diff against HEAD `87daf8bd5d36732f7cd29ebdda4653910333e9e5`, after prior source and counter-fix reviews. **No blocking finding in this integration.** This records source/test-oracle review only; root's final matched build and execution are separate.

## Route and invariants

`mesh_surface_hold_plan_vertex_array` now calls the actual RD direct-VB preflight after constructing exact original source/default buffers and rejecting null roots, while the original Surface version lock remains held, before assigning caller output. RenderingDevice already friends RendererRD::MeshStorage; no public API or additional friendship is introduced. Original source lease and immutable Surface hold keep its exact retained/detached Surface alive. Surface-before-RD lock order matches the real factories; callers remain required to enter outside Registry/RD locks. RAII unlock runs on preflight failure.

The normalized scratch result is deliberately discarded. The published plan retains raw attributes and original buffer/offset values, preserving implicit bindings and the real format cache key. Existing exact VA/format fields are retained only on success; all failures leave caller plan at its entry-reset default. Direct-VB errors propagate unchanged. This closes the previously orphaned helper route without turning preflight into a hold, publication or new source issuer.

Detached/frozen behavior remains coherent: an existing matching key may be described and checked; missing key under output-directory freeze returns ERR_UNAVAILABLE before new work. A retained but explicitly RD-retiring backing is conservatively refused for new planning; that does not revoke already accepted physical use. Defaults are now required to be actual eligible used VBs rather than merely nonnull RIDs. Header comments correctly state direct-VB validation and deferred existing VA/IA/full closure validation, and still require retention/revalidation before factories.

## New route tests and friend helpers

The three friend-only additions read original VB hold count, read device hold count, or mutate the original VB external-access flag. They do not create owners or publish custody; no production accessibility is expanded. The helper methods dereference known real owner entries, following the existing fixture's size helper. Their call sites are after successful original surface acquisition, and normal/current VBs were created by the deterministic fixture driver. Future allocation-failure injection would require guard logic before these helper calls; it is not part of this fixed driver case.

The route tests now distinguish a nonnull wrong-type real storage buffer from a missing default, and require ERR_DOES_NOT_EXIST from the real RD VB-owner check. The temporary storage resource is freed and the original normal default is restored before later operations. Temporarily marking the real normal VB externally accessible must produce ERR_UNAVAILABLE, clear output, and recover to OK after restoration. Shortening current VB owner metadata by one byte must produce ERR_INVALID_PARAMETER even for an existing cached VA key; size is restored to the fixture's original exact allocation size before cleanup. In this fixture, ordinary count3/stride12 requires all36 bytes; external Indexed144 count3 requires all432 bytes across its complete attribute set, so the one-byte-short boundary is meaningful for both cases.

A fresh ordinary plan asserts raw implicit binding remains UINT32_MAX; external attributes retain explicit0. Existing VA/format assertions, format-call count, VA count, Surface version count and immutable hold count assertions remain. New snapshots compare the real selected VB and device hold counts around route calls, while the actual default VB remains at zero holds. These prevent accidental acquisition from hiding behind the original Surface hold's nonzero baseline. Temporary metadata changes are restored and no fallible assertion causes an early exit before restoration; CHECKs remain nonfatal and ERR logging suppression is restored immediately around expected noisy failures.

The prior private-preflight counter-oracle fix remains intact. Its before-injection checks catch resource-only increments before tests overwrite saturation metadata.

## Evidence and limits

Read `plan-route-red-execution.json`: recorded command is the headless `*NativeVertexArrayPlan*` test filter, exit1, stage `plan without direct RD gate`. This is recorded author execution evidence that the new assertions reject the unconnected route; reviewer did not execute it or independently pin the red host. Actual final green and neighbor/runtime results belong to root. No claim of GPU success follows from this recorded CPU red.

Remaining scope is explicit: direct raw used-VB structural/eligibility checking, not full G/closure, existing VA/IA authority, selected index range/history, detached-directory aggregate limits, driver format support, material/profile qualification, public transport or Host COW. Empty-output assertions sample fields while the complete reset follows directly from the existing entry assignment; no incomplete result is published on failure.

Reviewed live file SHA-256 pins:

- `servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp`: `aa9432b0c60cc7f8dd7191ba689989225db615bd21d5d6ea6979d5a1ce40f30c`
- `servers/rendering/renderer_rd/storage_rd/mesh_storage.h`: `690ded3ab440d0399e3eaa0fe85456437f16cae583e2dcb28493755e48eae863`
- `servers/rendering/rendering_device.cpp`: `85af989923afc2851990c3c06e8c829b41d3117b94ab4cf2329157a38459a2ca`
- `servers/rendering/rendering_device.h`: `31efa34b295ac21dfcea509b4879fd3a8fa4ac421356552f2110d0fe9f57ec0e`
- `servers/rendering/rendering_device_immutable_hold.cpp`: `19dcec89608d5e85e391d310a764496983ad2a6560043e4015b2aa4e01551489`
- `tests/servers/rendering/test_mesh_storage_external.cpp`: `c72832cb6069fa020a557e8e70bcedb3597a5fc8e034a7ac025fc02c923fd02e`

Only this review file was written. No production edits, build, compiler, runner or test execution by reviewer.
