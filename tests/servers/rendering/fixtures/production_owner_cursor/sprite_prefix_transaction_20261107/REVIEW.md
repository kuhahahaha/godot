# Prefix transaction review

Reviewer: independent Sol subagent. Scope: baseline HEAD 55f00ae609 and the upcoming closed-row pipeline-prefix transaction. Read-only source review; no source edits, compilation, CPU/GPU execution, staging or commit by this reviewer.

## Baseline facts

- `production_output_owner_bind` rejects any nonzero next_action, graph_count or native_count before checking its already-installed owner tuple. It cannot be reused as an action continuation authenticator.
- `_production_output_owned_reserve` keeps the existing physical-context/pin admission. With action/native progress it requires `production_output_action_pin_matches_locked`, which accepts only the exact current NATIVE_OUTPUT RESERVING action, original source and owner tuple. Successful Acquire changes the action to BOUND and increments next_action, so that BOUND pin does not satisfy the factory gate.
- Reserve errors occur before assigning its output group serial or changing disposal cells. Successful reserve is the first effect and must retain original surface custody even if no native RID was created.
- Baseline prefix entry expects the caller to have opened the row before owner_bind/reserve. If either refuses, no `surface.preparation` is installed; slot OBSERVE cannot close that row, and `return_preparation_row` refuses `!source_sealed`. Generation readiness also waits for the finite row slab. This is a real metadata-release obstruction for a future opener, though no production opener exists at baseline.
- Slot OBSERVE first services `surface.pipeline_owned_group`. It refuses unresolved geometry/material prefixes, clears only its own attached preparation's pipeline metadata after group return, then returns the stable row. `return_preparation_row` requires exact slab address/owner/generation/lease and no open/pipeline/geometry/material state. These rules must remain strict.

## Required transaction boundary

The narrow solution is to accept an original closed row and a nonzero resource bound, authenticate its exact stable association and pristine state, reserve through the original factory, and only then open/attach it. Rejection must not change the row or surface; it must not cache the rejected physical context. An already-owned group, created pipeline, active hold or unknown prior native attempt must be refused intact. There is no need to weaken return-row checks, manufacture BOUND action admission or infer a renderer pass.

## Final diff review

Reviewed Astra READY's five-file working diff: `particles_storage.h`, `particles_storage_retire.cpp`, `test_external_output_custody_bridge.h`, `test_external_output_owner_state.cpp`, and `test_production_request.cpp`. No blocking finding in this scope.

- The entry now accepts a closed row and explicit nonzero bound. Exact owner/generation/slab/address/lease checks precede factory entry. New work requires an admitting generation, RESERVED slot, and both publication/use windows open.
- Removing owner_bind does not remove the original authority check: authentic Acquire already calls `production_output_action_owner_bind_locked` before row installation, and the unchanged owned factory reauthenticates the physical invocation, exact pin and current RESERVING action under the registry lock. Successful Acquire's BOUND action advances next_action, so later BOUND reuse is refused.
- Pristine checks preserve existing surface/preparation links, groups, roots, active geometry/material holds, prior cache query and cached physical context. No failure path mutates the row or caches the attempted context. Factory success retains the actual group in the original surface before opening/attaching the row, with no fallible operation between those assignments.
- Retire and return-row refusal rules are unchanged. No allocated/unknown native prefix is erased by this patch.
- The real acquired-slot tests call the actual entry for zero bound, absent context, current-context BOUND and repeated BOUND refusal. They verify closed row/surface state, no cached context, original BOUND action, unchanged request frontier/revision/group serial/native/graph counts, allocation/free counts and command count. Existing later owner drain still checks returned rows. They do not fabricate RESERVING state or renderer selection.

Validation limits: source review only. New signature requires old-source compile failure for tests-first and is not behavioral RED evidence. No reviewer build/test/GPU run. No authentic RESERVING success path, owned-prefix failure recovery or unknown-native-state runtime case is newly exercised, and no production caller exists. This review is not runtime qualification, a success-path preparation claim or public Sprite admission.
