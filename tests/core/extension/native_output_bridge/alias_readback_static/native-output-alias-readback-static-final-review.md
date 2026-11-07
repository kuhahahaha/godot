# Alias readback static admission: author self-review

This is an author self-review of the active three-file change, using stored pre-change source snapshots and read-only source comparison. It is not the separate independent Sol review. I did not build, execute tests, run Git, or edit active source. Root owns runtime execution and final integration.

## Result

No blocking functional finding in the reviewed static-admission plus explicit runtime-refusal scope. Freeze validates the exact unbound plan without creating an alias, witness, physical-return object, readiness state, or action progress. The READBACK branch explicitly returns ERR_UNAVAILABLE for an output alias before capture or native effects. Ordinary READBACK follows its unchanged existing path. This does not establish complete alias readback, public Instance192 support, GPU execution, or physical completion.

Reviewed active source identities at comparison time:

- core/extension/gdextension_production_request.cpp: `9c689a3f4d1808fd6bff4b6d8938a4412364117c1f01463a391eebabaae3cfb9`
- core/extension/gdextension_production_action.cpp: `a0280d0b224b7b24e49f2b621fbb1bccdf9dd1dfe0039543507a91a31c84a248`
- tests/core/extension/test_production_request.cpp: `be86c4c0b419545f8a535df42d280aaff6d652496271b910e6f71f498c339f62`

## Source checks

The request diff is exactly the static alias branch replacing its previous unconditional ERR_UNAVAILABLE. It validates original request/resource identity and serial, STORAGE/OUTPUT_ALIAS ownership, exact NATIVE_OUTPUT producer, prepared source, exact prepared READBACK action/slot, nonempty bounded READ window, role generation/extent and exact role WRITE row. It only reads those fields; the preexisting final `r.frozen = true` is the sole success-state transition. The real alias factory/binding functions are not called. Earlier producer/copy/resource bounds checks precede the new dereferences.

The action diff is exactly one six-line alias READBACK refusal, immediately after acquiring the existing readback cell. It precedes `readback = &cell.capture`, capture-field writes, transfer staging, capture_inflight, production_recording_request, append-effect entry, Registry unlock and `_buffer_get_data_async`. The guard returns a rejected append with ERR_UNAVAILABLE and cannot advance next_action or issue a graph copy. Its indices rely on the existing reserve/prepare/freeze validation, as did the previous branch. Existing earlier resource/budget checks may return their original error first. The ordinary-readback branch, RD permission gates and KnownPrefix automatic cancellation are unchanged. Even a future successful real internal acquire cannot bypass this explicit alias refusal.

Removing the new helper and new TEST_CASE from the active test reconstructs the frozen pre-change file byte-for-byte. All 86 existing TEST_CASEs remain, including all three GenerationSelection cases, the GenerationGuardRefusal case, and their implementations. No existing test cleanup was added or removed; unified diff alignment can make the adjacent cleanup appear moved.

The source-helper negative probes restore backing serial, expected source generation and source access before leaving their scope. Each freeze probe saves and restores writer/copy declarations, role, readback declaration, READ use, internal copy slot and source-prepared flag. The ten malformed cases precede the final valid case, so no successful frozen state can mask a later negative. CHECK failures do not skip restoration. The final valid case leaves the valid request frozen for original cancellation/cleanup. Tests do not set BOUND, next_action, witness fields or readback return/progress. Their numeric particles identity is explicitly schema metadata, not a created owner.

The test allocates only its actual original source, checks factory-attempt count does not increase during freeze probes, checks graph/native/readback counters remain zero and alias imported remains false, then cancels, retires/observes/releases the actual source and releases the original V2 request. Frame/driver IDs initialize a CPU fixture and do not constitute completion evidence.

## Nonblocking precision notes

1. The request comment still says the real factory witness “is checked at READBACK record admission.” In this batch admission is unconditionally refused for aliases. Prefer “READBACK recording remains gated pending real factory/witness integration coverage” so the comment matches the implemented boundary.
2. The replacement private-witness assertion correctly uses the original RD observer without adding friendship. `_native_disposal_observe` returns ERR_UNAUTHORIZED without writing Report for an empty witness, a foreign RD/RT, or an invalid main-instance context; Report::state defaults to EMPTY. Therefore that assertion alone is not proof of an empty witness. Describe it as original-observer refusal of the unbound slot. The stronger no-fabrication conclusion rests on the pure-read freeze implementation plus unchanged factory-attempt count, imported=false and zero effect/progress counters.
3. This helper directly tests static freeze and source predicates; it does not exercise the runtime ERR_UNAVAILABLE gate via a real bound alias. That is an explicit remaining runtime-coverage limitation, not evidence of full readback support.

## Execution evidence read, not produced here

Root's stored RED r2 JSON records exit 1 for the focused OutputAliasReadbackFreeze case, binary hash `b884db0616c9bd122511459e7d2637d5ea19b0bd3b8c511d50a3d73b40d1fbe7`. Root reported 1 case, 225 assertions, 9 failures; I did not independently inspect that assertion transcript.

The stored green-r1 and combined-r1 JSONs record exit 0 with binary hash `35dc9524bafcfc8d5bf4f7ee1da32826eceb98b729c9aee6a1863c6d630aa0ae`. Those JSONs explicitly limit their scope to CPU/dummy execution and require diagnostic/assertion inspection. I have not independently rerun or promoted them to GPU/physical evidence. Root should bind final claims to its full logs and latest source identity.
