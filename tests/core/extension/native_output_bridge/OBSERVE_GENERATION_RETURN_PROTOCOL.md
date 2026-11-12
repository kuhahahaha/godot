# Original OBSERVE generation return protocol

This is the native terminal projection/consume component, not a new owner or a logical E05/U09 ACK. Original Create/Acquire/Seal/Cancel/Withdraw/Poll remain the six RS operations. The native procedural transport below extends original OBSERVE; Poll remains read-only and is never an ACK.

## Typed entry points

```cpp
GDExtensionDeviceSessionResultV1 device_session_production_observe_v3(
    GDExtensionProductionRequestRefV1 request,
    const GDExtensionProductionServiceBudgetV1 *budget,
    GDExtensionProductionObserveResultV3 *out_result);
GDExtensionDeviceSessionResultV1 device_session_production_output_return_consume_v1(
    GDExtensionProductionRequestRefV1 request,
    const GDExtensionProductionOutputGenerationRefV1 *expected,
    GDExtensionProductionOutputGenerationResultV1 *out_result);
```

Both use the original V2 request contract and request-owning thread. No protected RECORD physical context or new draw is required. ObserveResultV3 has ABI version 3; unchanged ServiceBudgetV1 and new GenerationRefV1/GenerationResultV1 use ABI version 1. All headers require exact sizeof/reserved=0. Input and output objects must be aligned and disjoint. Consume leaves output unchanged on failure.

ObserveResultV3 contains `{abi, report:ProductionReportV2, output_generation:ProductionOutputGenerationResultV1}`. The terminal is copied directly from the original `ProductionControlOutput::output_generation`; it contains:

| Field | Original source |
| --- | --- |
| registry_instance, request_serial | Original Registry/request |
| control_serial | Original OBSERVE reservation/control serial |
| root_serial | The returned original generation ProductionOutputPin serial |
| particles, generation | Original ExternalOutputOwner generation identity |
| root_source | Exact immutable ProductionReportV2 captured at generation return |
| unused_slots, returned_slots | Original owner generation_returnable counts |
| terminal_kind | 0=None, 1=GenerationReleased; projection of original native_released |
| native_released | Original native_released bool, projected as u32 0/1 |

There is no new generation token or per-slot native lease minted by this API. The aggregate terminal's original native root is root_serial. C must preserve each original Acquire result's request_serial/action_ordinal/lease_token and its Pending/generation association; those fields cannot be reconstructed from Poll or scalar camera data. The original generation release gate already requires every admitted slot/native group/borrow/history association to be returned. The native generation terminal does not replace C's separate source/read or code obligations.

## C/B call and acknowledgment order

1. The actual original Pending owner retains its generation, original request and acquired native lease association. It closes future publication through original Cancel/Withdraw under the existing admitted route; B forwards request_close/service_owner/drained without clearing Pending.
2. Through the original service owner, call observe_v3 with the original bounded ServiceBudget. ERR_BUSY schedules/retries only original OBSERVE FIFO work. No simulation/RECORD/draw is added. Nonterminal turns retain legacy service consumption behavior.
3. A RETURNED generation terminal is projected with terminal_kind=1/native_released=1 and remains RETURNED. Zero-budget repeated observation can peek that exact result without scheduling, mutation or refreshed authorization. Original report diagnostics remain visible; native terminal must not be confused with whole logical completion.
4. C first compares the returned registry_instance/request_serial against its original Create request ref and particles/generation against its successful original Create result stored in Pending. CreateResultV1 has no root_serial or OBSERVE control_serial: this actual returned OBSERVE projection is the public point where C first learns those canonical native serials (unless another explicitly matched original internal route already supplied them). C must not be required to precompute or preknow either serial. After the known request/generation association matches, retain the canonical complete projection, build GenerationRefV1 `{abi, registry_instance, request_serial, control_serial, root_serial, particles, generation}` using all six values from that exact result, and call output_return_consume_v1. All six fields are checked atomically against the original RETURNED OBSERVE cell. Wrong tuple rejects without consuming. Success transitions only that cell to OWNER_CONSUMED and returns the canonical complete terminal; replay returns ERR_DOES_NOT_EXIST.
5. That consume is the native result ownership acknowledgment only. C then applies the original E05/source/read ACK and U09/code return obligations through their original owners. It may reclaim its Pending/row only when those original obligations and this actual generation terminal are satisfied. Native particles_free follows Released and destroys the placement owner/returns its metadata allocation; original request release/drained remains separate. No Poll, elapsed time, refcount or lack of a draw substitutes for any step.

Old observe_v1/v2 still return ERR_UNAVAILABLE on a returned generation terminal only for their respective matching V1/V2 request contracts, preserving it for a consumer that can receive the whole result. A semantic version mismatch rejects first (for example, observe_v1 on this V2 request returns ERR_INVALID_DATA). No V1/V2 DTO size is changed.

## Build and validation

Canonical schema: `core/extension/gdextension_interface.json`. Regenerate `core/extension/gdextension_interface.gen.h` using the existing make_interface_header/SCsub path. The fixed SDK must regenerate its matching `gdextension_interface.h` from the same schema and add loader entries for the two named procedural function typedefs. The six RS bindings/classes and extension_api.json do not change. No generated SDK source is edited by this Godot slice.

New selector: `[SceneTree][ProductionRequest][OutputObserveV3] public original generation terminal consumes without next draw and rejects wrong tuple replay`. It reuses actual SceneOnly Create/withdraw and original OBSERVE FIFO, validates canonical source tuple plus every wrong identity field, repeated zero-budget peek, old v2 refusal, exact once consume, metadata still pending after native Released, actual particles_free then request drain. Fake native driver/CPU test only; no GPU rendering qualification. Removing either new registration is a test failure before fixture allocation. This author ran compiler-command-based syntax checks for request.cpp/interface.cpp/test_production_request.cpp and diff check; parent owns unified build and execution. No runtime RED/GREEN is claimed here.

## Original Acquire action and C forwarding protocol

The existing exact entry points are:

```cpp
GDExtensionDeviceSessionResultV1 device_session_production_prepare_output_v1(
    GDExtensionProductionRequestRefV1 request,
    const GDExtensionProductionOutputPrepareV1 *prepare);
void device_session_output_acquire_v1(
    GDExtensionProductionPhysicalContextRefV1 physical_context,
    GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal,
    const GDExtensionOutputAcquireInputV1 *input,
    GDExtensionOutputAcquireResultV1 *out_result);
```

`action_ordinal` is the ordinal of the actual original V2 `GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1` action declaration. Request reservation requires each action's ordinal to equal its accepted action-array index. Its canonical_suboperation separately lies in the original operation's canonical range; it is not the ordinal. C must use the original compiled/reserved action mapping (including its exact first_use/use_count and source_recipe_substep). Never derive this ordinal from canonical_begin, source/output logical ordinal, a constant 0/1, or whatever request frontier happens to be current.

C prepares that same action once via ProductionOutputPrepareV1. Required original fields are action_ordinal, matching canonical_suboperation/source_recipe_substep, checked source_resource_slot/source_use_index/source_generation anchor, the complete contiguous first_source_use/source_use_count READ span, prequoted read_capacity/read_graph_capacity, three output roles, and the exact immutable AcquireInputV1. All source backing/count/attribute/ID/validity/gate uses stay in their original accepted span; the single anchor does not grant them. Role order is current_records=1, aligned_previous_records=2, output_meta=7. Each nonempty role has the original OUTPUT_ALIAS resource_slot, WRITE use_index, allocation_generation and exact byte extent. Absent roles have bytes=0, UINT32_MAX locators, generation=0. Protected commands/status are never supplied as writable roles.

Prepare is an original request-owning operation before record; Acquire runs only inside that request's admitted protected RECORD callback on the main RD render thread. The physical_context is this exact callback's live frame serial. C's payload→binding→RT→typed apply forwarding must carry the original Pending/output selection, request ref, action ordinal, prepared immutable input and role associations unchanged; B/A must not reconstruct them from latest frame state. First acquire requires request.next_action==action_ordinal, all relevant lists closed, original source backing valid and owner metadata/generation selection prepared. The executing original action may be reached after other accepted actions; there is no assumption that Acquire is action 0 or 1.

AcquireResultV1 projects `{abi,status,custody,state,lease_token,writable_count,reserved,writables[3],resource_slots[3],use_indices[3],request_serial,action_ordinal}`. Success means the original action is internally BOUND (not a public enum named BOUND); the observable lease state normally starts RESERVED, whose wire value is 2. Native state is separate from producer completion/Seal/RecordedOrdered. Store the returned exact 32-byte lease token with the original Pending, particles/generation, request_serial and action_ordinal. Each active writable descriptor's role/RID/allocation_generation/byte_length/stride/access must join the prepared role and returned resource_slot/use_index; returned RIDs do not import another request's authority. Capacity0 can have no writable descriptors while still retaining an original lease. Inactive locators remain UINT32_MAX.

The original native lease token consists of the original registry instance/request serial/action ordinal/slot lease serial in current implementation, but callers treat it as opaque and never manufacture or decode authority from it. Duplicate exact BOUND acquire only reprojects those same original roles after live-source/backing/admission checks; RETIRING/UNKNOWN returns no new writable permission. Any error with RETAINED_PREFIX/UNKNOWN custody requires retaining the original Pending and its real prefix; status error alone is not no-effect proof.

There is no field literally named `frame_meta` in either Core Acquire DTO. C's existing frame metadata/OperationCorrelation remains owned by its actual Pending/binding and must accompany that same request/action through forwarding. `output_meta` is the actual role-7 owned buffer and is not a substitute for that original frame metadata. These two associations must not be collapsed or re-created. Successful returned descriptor association feeds the actual original producer binding/record; original Seal then consumes the same native lease and exact acquire common tuple. It does not permit a later callback with a copied token to choose different payload/resources. Poll never binds or ACKs it. The generation return protocol above is the closing aggregate for these same rows, while C retains and applies its original per-row source acknowledgments.

ABI rejection regression additionally corrupts version/sizeof/reserved on both inputs, passes misaligned input and output extents, and overlaps aligned input/output extents at the real RETURNED terminal. Each rejection checks byte-for-byte unchanged output and unchanged control serial/state, request revision and complete immutable terminal. Parent owns incremental runtime validation.

## Existing typed Poll: exact fields and generation/slot association

```cpp
void device_session_output_poll_v1(
    GDExtensionProductionPhysicalContextRefV1 physical_context,
    GDExtensionProductionRequestRefV1 request,
    const GDExtensionOutputGenerationInputV1 *input,
    GDExtensionOutputPollResultV1 *out_result);
```

`GenerationInputV1` is exactly `{abi, particles:u64, generation:u64}`. Both IDs must be nonzero. Its ABI is version1/exact sizeof/reserved0. The public leaf checks aligned complete disjoint input/output extents, then calls the original Registry `production_output_poll`. This input is the original generation identity; it is not an Acquire lease token or a new generation grant. There is no slot/lease token argument and no implicit latest-generation selection.

`PollResultV1` is exactly `{abi, status:OutputStatusV1, generation:u64, state:u32, custody:u32, known:u64, possible:u64, complete:u64, physical_returned:u64, request_serial:u64, action_ordinal:u64}`. Its ABI is version1. `status` contains original Godot error plus detail length/pointer. `state` uses OutputGenerationStateV1: INVALID0, CONFIGURED1, ACTIVE2, WITHDRAWING3, RETIRING4, FAULTED_RETIRING5, RELEASED6. Custody is NO_CUSTODY0, ACCEPTED1, RETAINED_PREFIX2, UNKNOWN3; this Poll returns UNKNOWN when it observes uncertainty, otherwise NO_CUSTODY for an actually Released owner, otherwise ACCEPTED. RETAINED_PREFIX remains an Acquire observation category and is not fabricated by Poll.

Validation follows the actual original owner paths:

1. `production_physical_request_locked(..., append=false)` authenticates the exact live protected RECORD callback/context, request, original holder/code registration, device epoch and render thread. Poll is not callable with a cached frame serial, from arbitrary owner code, or from OBSERVE. It requires the main RD and no concurrent capture; lock contention returns Busy.
2. Resolve the original particles RID owner and require exact external generation, device and Registry. The original generation root/request serial must be present. A live generation pin must match its original owner and request; an empty pin is tolerated only when that owner is already truly RELEASED. Cold original root/request identity remains solely to correlate that terminal owner until actual particles_free.
3. The supplied request must be the exact original generation request **or** an actual slot request in that same owner. For a slot-associated request, Poll validates original Registry/cold released identity and checks the slot's original action ordinal plus the request action's particles/generation/slot/lease incarnation. This verifies the original internal slot/native-lease association; callers do not supply or synthesize a slot token to enroll another request. A foreign request fails even when particles/generation numbers match.
4. `request_serial` is the authenticated supplied request. `action_ordinal` is the unique matching original slot action, or UINT64_MAX if there is no unique matching slot action (generation-root-only observation or multiple matching actions). It is never a replacement action selector. `known/possible/complete` are the original supplied request's obligation report, while `physical_returned` counts actual output resource returns across the whole validated generation, including retained cumulative receipts after storage destruction/slot reuse. Thus counters of different scope must not be combined into a made-up completion test.

Poll performs read-only actual native-disposal observations and reports current owner/custody. It neither advances original action/record/retirement frontiers, services queues, schedules controls, appends work, closes a lease, consumes RETURNED, acknowledges source/code obligations nor frees metadata. Status alone, zero counters or lack of another draw is not a Released proof. UNKNOWN preserves custody and its original cause.

A successful Poll with state=RELEASED6 describes an already-released original native generation that remains observable while its particles metadata owner still exists. It is **not** the original immutable OBSERVE result and carries no control_serial/root_serial/root_source terminal snapshot. C must use the observe_v3→exact output_return_consume_v1 protocol above for that original native result ownership acknowledgment and then discharge original E05/U09 obligations before row reclamation. Poll never substitutes for this consume, and a new RECORD/draw must not be introduced merely to Poll a closing generation. The no-next-draw route is the original bounded OBSERVE service/terminal consumer. After actual particles_free, this Poll input is stale and cannot address a reused RID generation.


## Caller-supplied V2 action declaration boundary

The exact existing reservation signature is:

```cpp
GDExtensionDeviceSessionResultV1 device_session_production_reserve_v2(
    GDExtensionProductionAttachmentRefV1 attachment,
    const GDExtensionProductionOperationV1 *operation,
    const GDExtensionProductionCapacityV1 *capacity,
    const GDExtensionProductionActionV2 *actions, uint32_t action_count,
    const GDExtensionProductionResourceV1 *resources, uint32_t resource_count,
    const GDExtensionProductionUseV1 *uses, uint32_t use_count,
    const GDExtensionProductionReadbackV2 *readbacks, uint32_t readback_count,
    GDExtensionProductionRequestRefV1 *out_request);
```

These action/resource/use/readback arrays are supplied by the actual original E09 production caller; Godot does not construct an ANGE action plan or issue source recipe meaning. The interface forwards those arrays to Registry::production_reserve_v2, which validates and copies them into the one original request arena. This document establishes the receiving contract, not the existence or authority of a particular ANGE producer that has not been inspected.

GDExtensionProductionActionV2 is a headerless typed row containing action_ordinal:u64, canonical_suboperation:u64, source_recipe_substep:u32, source_recipe_reserved:u32, action_kind:u32, physical_unit:u32, first_use:u32, use_count:u32, list_boundary:u32 and readback_slot:u32. `actions[i].action_ordinal == i` is required. Canonical values must lie within the unchanged operation range and be nondecreasing; V2 allows repeated canonicals/missing canonicals, so canonical_begin+index is not the action identity. first_use/use_count denotes a checked contiguous subrange of the supplied original uses array: first_use<=total use_count and row.use_count<=total use_count-first_use. Source recipe reserved must be zero. A native-output action specifically requires kind=NATIVE_OUTPUT, physical_unit=UINT32_MAX, readback_slot=UINT32_MAX and list_boundary=CONTINUE. PrepareOutput then verifies its original source READ span and output role WRITE uses lie in that same frozen row's use range.

Source anchors (current checkout; line positions may shift with unrelated edits):

- CreateResultV1 layout is in canonical interface JSON/generated header: `{abi,status,particles,generation,capabilities,reserved}` only. `particles_storage_external.cpp` final Create receiver assigns particles/generation/capabilities at lines527–532; no root/control export is present.
- `gdextension_production_interface.cpp:280–283` forwards the exact caller arrays without inventing them. `gdextension_production_window.cpp:307–326` authenticates the original attachment, passes caller arrays into production_reserve_metadata_rows_locked with contract version2, and returns `{Registry instance, original request serial}`.
- `gdextension_production_request.cpp:598–602` checks action ordinal/index, canonical range/order, first_use/use_count bounds and native-output sentinels. Lines796–804 copy original declarations for action/resource/use rows into the request arena.
- `gdextension_production_output.cpp:297–415` validates PrepareOutput against that original frozen action, original source span and output alias role rows. `production_output_acquire` starts at416 and checks prepared equality plus actual next_action before the first acquire.
- `Registry::production_output_generation_return` assigns the real terminal registry/request/control/root/particles/generation from its existing original request/control/pin (currently request.cpp:2380–2386). observe_v3 copies that immutable terminal; consume compares the complete canonical tuple to the same original RETURNED cell before ownership consumption. No serial is reconstructed from Create, time, frame counters or token bytes.
