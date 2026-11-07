# Instance192 native metadata quote / Create capacity transport

2026-11-07. Source and canonical-contract analysis only. No source/ABI/candidate edits, build, execution or Git. Canonical directory supplied by root resolves to `/Users/macbookair/Documents/Work/Dev/ANGEParticleDesign/docs`; references below are to that same canonical content, not a copied contract. Selection candidate files are owned separately by Astra.

## Recommendation

Add an explicitly versioned native metadata requirements query and a new Create DTO carrying the same finite technical limits. Keep existing CreateV2 layout/SceneOnly semantics intact. Use one native checked layout function for query and Create. Existing production quote/accept/reserve remains the sole request-arena admission path; the new requirements query reports native bytes, never grants account credit or a source/selection lease.

Do not derive native read-use capacity from `max_views`, `max_inflight`, pass-mask popcount or particle capacity. Do not treat `ProductionCapacityV1.readbacks` as native readers: it counts request readback declarations. Do not ask B to reproduce sizeof native classes. Do not convert zero/missing limits to invented nonzero defaults.

## Canonical requirements actually established

- `contracts/b4-native-output/OUTPUT_CONTRACT.md:33`: compiler supplies config max_views<=8, max_inflight<=4, capacity<=2^20; defaults belong to compiler, never K. Technical defaults are overridable and do not guarantee U08/world/device allocation.
- Same file:56: acquisition reserves current/previous/guard/native-array/surface/material/read-lease bytes before success. Exact original source/request is retained on retry. No GPU actual-count readback determines allocation.
- Same file:74: zero capacity has no writable descriptor and an explicit empty packet. It does not erase protected control/status/retirement obligations.
- Same file:92–96: Reserved/Writing/Sealed/Adopted/Retiring lifetimes remain charged; Busy before work cannot evict active history or recorded uses; checked arithmetic and original native failure mapping apply.
- `VIEW_CONTRACT.md:13–21`: distinct views have disjoint write/result leases; reuse requires identical complete immutable association and all reader leases. Repeated depth/shadow/color passes do not imply one read-use or advance history multiple times.
- `E14_E16_PRODUCTION_MECHANICS.md:13–16,78–96`: max_inflight includes every live/retiring request row; one eligible tuple per view plus already admitted uses may remain. Original quotas bound retained generations/scratch; no larger private pool or unbounded retired side list. Each request row retains its original operation/reservation and native write/read lease.
- These canonical files do not specify a numeric maximum of simultaneously retained native read uses or a finite graphs-per-read formula. They also do not identify simultaneous ViewCell or SurfaceCell consumer leases one-for-one with configured view or surface state. Those limits must be supplied by the original admitted technical plan, not invented here.

## Actual implementation and transport gaps

- `GDExtensionOutputCreateInputV2` is exactly `{abi, config:CreateInputV1, metadata_resource_slot, reserved}`. Its public receiver calls `_external_create_typed(..., views=0,reads=0,surfaces=0)`. Both native receivers retain SceneOnly gates. `particles_external_scene_metadata_bytes()` sizes only that zero-directory case.
- Native `plan_metadata_layout(views,reads,surfaces)` currently emits Owner, ViewCell[], ReadCell[], conditional MAX_SLOTS Storage objects, SurfaceBinding[surfaces], SurfaceCell[surfaces]. The single surfaces count currently serves both per-slot preparation bindings and consumer-selection cells. Config mesh/material selection rows and lease placement are a further addition owned by the selection implementation.
- Original generation view state is already `owner.views[MAX_VIEWS]`; slot state is `owner.slots[MAX_SLOTS]`. Directory ViewCell objects represent retained `ExternalPacketViewLease` consumers, not configuration view identities. SurfaceCell represents an `ExternalSurfaceSelection` consumer. Current surface/view acquisition is unavailable, so an exact multiplicity invariant cannot be inferred from a working public issuer.
- Acquire requires `slot_count * actual_flattened_surfaces <= owner.surface_capacity`; per-slot SurfaceBinding indexing uses that product. It separately checks action read_capacity<=owner.read_capacity and read_capacity*read_graph_capacity<=remaining request graph memberships. `prepare_output_v1` already transports action read_capacity/read_graph_capacity and validates their paired-zero and quotient bound before freeze. No public Create count currently installs the corresponding ReadCell directory.
- `ProductionCapacityV1` fields are request/arena counts: requests, suboperations, resources, resource_uses, graph_memberships, native_memberships, readbacks, return_cells, retained_payload_bytes, readback_bytes. They do not encode generation view/read/surface consumer directory counts. Production layout accounts these arena slices; the OUTPUT_METADATA resource is a separate physical CPU allocation, not retained_payload_bytes.
- Metadata create requires original frozen protected RECORD with empty frontier/graphs/native/effects, resource kind OUTPUT_METADATA, ownership CREATED_BY_OWNER, required_usage0; checks requested bytes<=accepted declared physical_bytes, and allocates the declared size. Thus new typed Create must enforce exact quote/declaration equality itself if that is the chosen new contract. Do not claim current generic metadata_create already enforces equality: it permits a larger accepted block.

## Derived state versus accepted consumer/read limits

Let V=config.max_views, I=config.max_inflight, P=V*I, M=the real native flattened selected surface count. All multiply/add/alignment calculations checked before narrowing.

| Quantity | Source | Rule |
|---|---|---|
| configured view state | config | V, already bounded <=8 |
| writable/history slot state | config | P=V*I, <=32; all retiring rows count |
| generation mesh-selection rows | actual config/native mesh resolution | config.surfaces.count, exact native identity retained by later Create |
| generation material/surface selection rows | actual flattened native selection | M, validate exact surface_materials.count==M |
| per-slot SurfaceBinding rows | config + actual native flattening | P*M; no guessed surface count from meshes.length |
| ViewCell consumer directory | accepted technical plan | explicit `view_lease_capacity`; only derive from P if original consumer contract explicitly guarantees the requisite maximum simultaneous leases |
| SurfaceCell consumer directory | accepted technical plan | explicit `surface_lease_capacity`; do not assume P*M bounds repeated cache/pass consumers absent an enforced uniqueness invariant |
| ReadCell consumer directory | accepted technical plan | explicit generation `read_lease_capacity`, including simultaneous already admitted retained readers |
| per-action read-use claim | accepted action prepare | existing `read_capacity`; <=generation read_lease_capacity; actual concurrent reservation must also check free cells, not only total capacity |
| per-action graphs per read | accepted action prepare/native qualification | existing `read_graph_capacity`; a separate graph obligation bound, not bytes or directory count |

It is possible to make the transport smaller after proving and enforcing a single-live-ViewCell-per-slot / single-live-SurfaceCell-per-slot-surface invariant. In that variant derive those directory counts from P and P*M and omit their input fields. Current canonical/source evidence does not establish those invariants, so silently choosing them now is not justified. The minimal safe transport for current independently counted directories therefore includes all three lease capacities. Splitting binding count from surface consumer count in the layout avoids repurposing either directory.

## Minimal fixed DTO proposal (design only)

Introduce `OutputMetadataLimitsV1 { abi; u32 view_lease_capacity; u32 read_lease_capacity; u32 surface_lease_capacity; u32 reserved=0; }`. Values are explicit accepted technical claims. Zero means no such future consumers; a profile operation requiring one must refuse before effects. They are not proof of original U08/account admission.

Requirements query input: existing exact CreateInputV1 config plus MetadataLimitsV1. The outer DTO has its own exact ABI/version/struct_size; borrowed config RID spans remain bounded and synchronous as in existing Create. No new numeric identities or callbacks. Query result: status, native layout/version, required_metadata_bytes:u64, required_alignment:u32, derived slot_count:u32, actual_flattened_surfaces:u32, per_slot_binding_count:u32, echo/normalized limits, reserved fields0. Counts are useful comparison diagnostics; native layout version prevents treating a prior engine layout as reusable. No opaque quote handle is necessary: recomputation is mandatory and no ownership is acquired by this size query.

New CreateInputV3: exact outer ABI3 + existing config + the same MetadataLimitsV1 + metadata_resource_slot + reserved0 + exact native layout/requirements projection (or its complete fixed quote DTO). Never enlarge or reinterpret CreateV2, reuse its reserved field, or accept header-size fallback. Create compares the quoted projection with a fresh checked native plan and the exact accepted OUTPUT_METADATA row before allocation. Source identities are revalidated and retained by the actual selection transaction; equality of layout/byte size alone is not a selection grant.

`read_graph_capacity` does not affect metadata layout and already exists on each prepare_output action. Keep it there for the minimal change. It must be supplied by the original accepted read plan/native qualification, then checked against that action's original request capacities; do not duplicate it in generation Create and confuse one generation's directory capacity with a per-request graph reservation. If a deployment additionally requires a generation-wide graphs-per-read ceiling, add an explicitly named accepted ceiling and enforce both at prepare/acquire; that is additional policy/transport, not a formula derived from V/I/M.

## Query -> accepted reserve -> Create -> acquire consistency

1. Original caller obtains real accepted limits/credit and immutable config input. Query runs only with actual same-device native storage authority/serialization; reading mesh/material containers is not safe merely because a raw RID exists. It performs checked sizing/validation, no VA/pipeline factory, no selection lease, no driver allocation.
2. Query's native bytes/alignment feed the accepted OUTPUT_METADATA declaration `physical_bytes`; original production quote/accept budgets its arena and native membership capacity. Original account admission must include the separate native metadata/physical bytes; size query is not that grant.
3. Caller reserves a zero-action generation Create request with one OUTPUT_METADATA row, return cell and spare original use row for independent generation pin. Freeze requires no fabricated imported/prepared bits.
4. Protected Create revalidates exact config/limits/actual flattened count, checked layout version/bytes/alignment, original metadata row, original device/selection identities, and finite placement. Refuse mismatches before native effects; no implicit resizing or counts fallback. Query does not freeze a mutable mesh/material; Create obtains actual CPU selection leases before publication.
5. Subsequent output requests use the actual Create RID/generation. They independently quote/reserve producer/native/read graph/native memberships and spare pin/group/member rows, then prepare exact source/roles/read limits. Creating generation metadata does not grant those later arena rows or native completion sources.
6. For each action, let R=prepared read_capacity, G=prepared read_graph_capacity, gp=already captured producer graphs. Require paired zero, checked R*G<=request.capacity.graph_memberships-gp, R<=generation read_lease_capacity, plus original native-membership bound sufficient for every graph. Also account guards/copies/other suffix graphs explicitly; the product check is not proof that remaining actions need zero graphs.
7. Reserve actual free read/view/surface cells transactionally before success; retain them through every actual recorded use/return. Total-capacity checks alone do not prevent overcommit across simultaneous requests/slots. On exhaustion Busy is valid only before effects; retained prefixes follow existing custody/error protocol.

## Required checks before enabling the public profile

Native quote/Create exact match and one-field mismatch for each limit/count/layout/version; zero limits versus attempted consumer; undersized/oversized declared metadata per chosen exact contract; stale selection between query/Create; flattening changes; products/alignment/UINT32/device range overflow; same generation across two actual requests; two views and retiring in-flight rows without eviction; bounded read graph claim with producer prefix; actual free-cell exhaustion; every unpublished selection-prefix cleanup and metadata/root return. SceneOnly V2 regression remains separate. Neither this transport plan nor CPU tests establish native shader readiness, actual draw, original final-root permission or GPU qualification.
