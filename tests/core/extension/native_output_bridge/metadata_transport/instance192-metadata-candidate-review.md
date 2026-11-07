# Instance192 metadata transport candidate independent review

Reviewer `/root/review_vertex`; read-only review of 01-metadata-transport.patch and 02-metadata-tests.patch, corresponding exact candidate/base sources and adjacent live owner/layout/context code. No active/candidate source edits, builds, tests or Git operations. All IDENTITY.json base/candidate entries matched the files read. Candidate JSON parses and has no duplicate named type or interface entry. Guard candidate remains frozen.

## P1 — nonexistent RenderingServer include blocks compilation

01 adds `#include "servers/rendering_server.h"` to candidate particles_storage_external.cpp:35. That path does not exist in this repository. The actual header is `servers/rendering/rendering_server.h`, already used correctly by particles_external_guard.cpp. The added query requires the complete RenderingServer definition, so removing the include would not be the correct repair. Correct the path in candidate source and regenerate patch/identity before applying/building. Root and Astra were notified; this reviewer did not fix it.

No second blocking semantic defect was found within the declared SceneOnly/zero-consumer-capacity first slice. The findings below bound that conclusion; they are not acceptance of unimplemented nonzero Instance192 layout or a compiled generated ABI.

## Query authority and effects

Public metadata_query_v1 takes only config+limits and has no production context or account argument. After outer/span checks it requires actual storage, then the native query checks valid V1 outer/limits ABI, reserved0, live RenderingServer/RD singleton, rs->is_on_render_thread and equality to rs->get_rendering_device. This is the same main renderer ownership pattern as the existing guard wrapper; it does not fabricate RECORD authorization. RenderingServer::get_rendering_device returns the global RD singleton, while local RDs do not replace it in normal operation. The absent storage case returns ERR_UNAVAILABLE; wrong renderer/thread returns ERR_UNAUTHORIZED without decode/selection.

The extracted decoder validates bounded config, converts/copies borrowed data and performs read-only native selection checks. It may allocate temporary CPU vectors/tag arrays, so “read-only” must mean no retained owner/native factory/account/root/selection effects, not zero heap allocations. The query itself calls no metadata_create, generation allocation, native factory or hold increment. It refuses render profiles before planning/returning requirements; supported SceneOnly input has no surface/material entries. Nonzero explicit consumer capacities refuse. Native plan_metadata_layout(0,0,0) supplies actual bytes/alignment, config.max_views*max_inflight is safe because decoder limits them to8 and4, and flattened/binding counts remain0. Query has no stable authorization-bearing quote handle; requirement projection can only be used as later checked data.

## Exact original accepted bytes precede producer effects

CreateV3 validates outer ABI3/size/reserved, requirements ABI1/size/reserved and real metadata slot, authenticates `_external_typed_context(context,request,true)`, then recomputes query requirements from the actual supplied config and limits on the same main RT. All projected layout_version, required bytes/alignment, slot count, flattened count and binding count must match the recomputed result. Limits are not separately memcmp-ed because the recomputation validates their ABI/reserved fields and only accepts all three zero capacities; the returned limits copy is the same checked input. Different nonzero capacities cannot sneak through an omitted projection comparison.

It decodes config again before effects, checks generation exhaustion, then calls original Registry::production_output_metadata_create(...,exact_bytes=true). Inside the existing Registry mutex, production_physical_request_locked reauthenticates the real protected context/request/device and resource row. Resource type/ownership/usage checks precede `(exact_bytes && bytes != declaration.physical_bytes)`, which precedes both resource mutation and Memory::alloc_static. Under- and oversized accepted declarations therefore fail before the physical metadata allocation, native_effect_entered or revision increment. The pointer/alignment is supplied freshly by native planning; accepted declarations currently have no separate alignment field, and Memory::alloc_static is constrained by the existing max_align_t planner/metadata check. A forged larger projection cannot change the bytes argument, which comes from recomputed quote b.

The original `_external_create_received` then replans and revalidates the authenticated exact lease before owner placement, generation increment and directory/RID/root installation. Any unconsumed local lease follows the original metadata_return path. SceneOnly has no selection leases/native geometry and no post-root newly introduced failure prefix here. Render profiles remain refused; this patch must not be used to enable them before separate generation selection/binding/consumer directory layout and failure drain exist.

There are no native callbacks between the two decoder invocations; borrowed caller DTO/span storage must remain stable for the synchronous API call, as for V2. This review does not treat concurrently mutating borrowed native memory as a supported input contract. No request/source authority comes from an earlier successful query.

## DTO versions and pointer ranges

New outer query/result, requirements and limits DTOs are V1; CreateInputV3 alone requires version3. Nested config remains the existing V1 and is independently validated by the common decoder. Input.requirements limits ABI/reserved are validated through the recomputed query. Unknown version/struct_size/reserved fields refuse before native effects.

Both new public leaves first check output and full outer input extents with alignment and overflow validation. They reject overlapping outer input/output without writing. The generic output_create_spans helper is changed from a fixed CreateResult reference to a template so it can initialize either result type, preserving the original V1/V2 behavior. It checks both nonempty RID spans for null/misalignment/extent overflow, then rejects output overlap without writing. Nested spans overlapping any part of outer input (including V3 requirement/resource-selector fields), or each other, produce CONTRACT only after disjoint output is established. Zero-length spans may have arbitrary data pointers and are never dereferenced. No new nested pointer-bearing object exists in requirements/limits. This retains the original outer-span handling, rather than checking only config subobject extents.

Both symbols are registered with generated-function-typedef static_asserts. The generic legacy lookup denies all device_session_ symbols, so these new names remain guarded-lookup-only without requiring a new manual deny list. Actual ABI code generation/compile has not been executed by this reviewer.

## Config extraction and V2 compatibility

Compared old and extracted decoder line-by-line: original ABI/ranges/limits, finite bounds and cull merge, profile-specific shape rules, geometry_bounds absence check, SceneOnly exclusion rules, surface/material RID copying, five fixed tag copies and validate_native_selection remain in their original order. The helper now returns bool instead of returning from the Create body, but each refusal keeps its prior error/detail. V1/V2 still authenticate context first and initialize output ABI before calling it. Their metadata slot handling, render-profile refusal, generation check, planner and receiver calls are unchanged.

The new exactness bool defaults false in the C++ declaration, and V2's existing call omits it. Existing V2 larger accepted metadata allocations therefore continue to be allowed. This is a private helper parameter, not a new public ABI argument. V3 requests equality intentionally. The pure span helper template does not change how existing calls instantiate or validate ranges.

## Directed tests and missing boundaries

02 adds a separate use_v3 switch through the existing actual protected SceneOnly callback and real generation retirement fixture, retaining all existing V2 cases. Its new case checks bad query outer version, UINT32_MAX consumer capacity refusal, successful query bytes/counts and unchanged revision, no-context Create refusal without new RID/revision, UINT64_MAX projection and layout-version mismatch refusal, successful exact V3 Create and the pre-existing bounded generation/metadata return chain. These tests are uncompiled/unrun, and the nonexistent include currently prevents building them.

The revision checks are useful observable refusals; they do not by themselves prove zero native allocations, which follows from source ordering here. UINT64_MAX is a forged projection mismatch, not native layout multiplication overflow. SceneOnly's zero capacities and fixed owner storage do not exercise nonzero Instance192 count products.

Important follow-ups before declaring the public transport verified: exact accepted metadata row sizes below AND above native required bytes (not just a forged projection), each required projection field/limits mismatch, wrong-thread/no-renderer query, complete new-leaf DTO/span alias matrix, legacy lookup refusal/generated typedef check, and V2 larger-declaration neighbor compatibility. Source currently implements those semantics, so absent tests are coverage limits rather than independently demonstrated bypasses. The key exact-byte predicate could be removed and all currently added V3 tests would still use an exactly sized declaration and likely pass; do not describe 02 as proving the new accepted-size rule.

A minor documentation issue: QueryInput description in candidate JSON mentions an original metadata resource slot and UINT32_MAX even though QueryInput contains no slot; QueryResult description is copied from Create's no-new-RID language. Correct those user-facing descriptions to query requirements semantics when finalizing; this does not change runtime authority.

## Disposition

BLOCKED ON SOURCE CORRECTION of the include path; no other blocker found in the supported SceneOnly first slice. After that correction this remains SOURCE-REVIEWED / BUILD-AND-RUNTIME-PENDING, with exact accepted-size endpoints and wire boundaries explicitly pending. No public Instance192 enablement or GPU conclusion follows.

Read time: 2026-11-07T02:08:28.925277+00:00

- `work/instance192-metadata-candidate/01-metadata-transport.patch`: `644c5175f429db5897e594a36759250ac09f305264de6996a12467f4ca056cbb`
- `work/instance192-metadata-candidate/02-metadata-tests.patch`: `8fbcaec631e7ba9586016287f1b4061809b8b8ced5c8d68cf1d52974fe13a508`
- `work/instance192-metadata-candidate/IDENTITY.json`: `28fa8a867e83af9c961cdf5e26251013a7d3ea6759e7159378696e6b09d3575a`
- `work/instance192-metadata-candidate/candidate/servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp`: `63d52b6df224dd095713ac0699eecefd3f135f964c6b2db10c3e19118fb1679c`
- `work/instance192-metadata-candidate/candidate/core/extension/gdextension_production_metadata.cpp`: `b960507daa21f11b91c7b7f6c3caa99f91635094decf5b1e72bb14f68786e20e`
- `work/instance192-metadata-candidate/candidate/core/extension/gdextension_interface.json`: `08c8037949094f3a9ca90f5542b999017fa012e285ab5678004de3b279ea226c`

## Applied R2 re-review

The live R2 source corrects the include to `servers/rendering/rendering_server.h`; the initial P1 is SOURCE-RESOLVED. No further supported-scope source blocker was found. This supersedes the initial blocked disposition, but compilation/runtime remain pending at this review; root owns the sole build/test lane. Guard candidate remains frozen.

The accepted-size cases now change the actual accepted resource declaration through `ordinal_initialize` by required−1 and required+1, then use the original protected RECORD callback and native V3 create. On rejection they assert ERR_PARAMETER_RANGE_ERROR, zero output RID/generation, unchanged request revision, no metadata allocation/owner head/native effect, no graph/native entries, and bounded original release cleanup. The required+1 endpoint is a meaningful counterfactual killer: removing the exact-byte clause leaves the old bytes>declaration check satisfied, so the original allocation/create succeeds and the new rejection/effect assertions fail. The −1 endpoint independently checks the lower endpoint but would already be refused by the old bound. No mutation experiment was run.

The V2 required+1 neighbor takes the default exact=false path and proceeds through the same actual adoption, withdrawal, bounded generation retirement and terminal metadata return fixture. Source/test path supports intended compatibility; no runtime success is claimed. Query JSON descriptions now describe the projection/read-only main-render-thread query. Complete new-leaf wire boundaries and wrong-thread/no-renderer coverage remain separate from these accepted-size tests.

Live-source SHA256 at 2026-11-07T02:14:09.803100+00:00:

- `core/extension/gdextension_interface.json`: `a7887c9a1f2880d03ed98aacdc27cf004dd6f454143da963a8bb8199c1c2ffcb`
- `core/extension/gdextension_external_output_interface.h`: `df09ab2a125036eeff2b8d2123e695f6e8134b5715d5143ff0c80360e5dd3e84`
- `core/extension/gdextension_production_interface.cpp`: `a414e170972c13295e821929f01a7ab23257895338451f99055bf59d74dd4d30`
- `core/extension/gdextension_library_exit_guard.h`: `ae8301f5df689601c3559bb048f32689ef9122437866b0da508fa7a73b8cb027`
- `core/extension/gdextension_production_metadata.cpp`: `b960507daa21f11b91c7b7f6c3caa99f91635094decf5b1e72bb14f68786e20e`
- `servers/rendering/rendering_device.cpp`: `605cbdc68ba68e97fa610de417b5619132511698259a257cf5e0a8c559c59ffb`
- `servers/rendering/renderer_rd/storage_rd/particles_storage.h`: `f98cbaa1cfa693e3642ef0663e917c3ee31beea77e8ea30492d07bf51fdecd2b`
- `servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp`: `bba97b76c8c277e0ed0beb976f76bd392170b210776d08a3dbaccc78aaf9f2e9`
- `tests/core/extension/test_production_request.cpp`: `f23ae73a4e7703d496e2a8b73fb0c629578b8fe5291341af771a5404a77b8d21`
