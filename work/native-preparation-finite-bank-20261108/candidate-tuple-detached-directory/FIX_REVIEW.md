# Focused independent fix review

Source-only review of current live changed seams; no builds or executions performed by this reviewer.

No actionable defect found in these exact fixes.

* `servers/rendering/renderer_rd/storage_rd/particles_storage_retire.cpp:674-682` now checks original geometry adoption/transfer, calls `_native_hold_return_directory` before `spend()`, and leaves explicit metadata begin/finish transitions behind `spend()`. This closes the concrete one-pass-budget starvation: the directory operation authenticates the same budget/usage and charges its own bounded pass (`rendering_device_immutable_hold.cpp:629-710`); the caller no longer consumes that sole pass first. Refusal preserves the directory binding, while successful return clears it, allowing the next turn to reach `_native_hold_begin_return` and record row metadata completion.
* `tests/core/extension/test_production_request.cpp:8266-8280` now frees any valid first VA before disposing the Surface on either VA-creation failure. The first branch also handles a valid VA paired with an invalid format check. The second VA is explicitly invalid on its failure branch. This closes the previously reported partial-construction leak without exceptions. No new production cleanup behavior is introduced.
* Immediate preflight after the deliberately failed raw factory now expects OK for failure and ERR_ALREADY_IN_USE for successful creation (`test_production_request.cpp:8520-8521`), consistent with a failed factory leaving its reservation available while a completed row subsequently becomes non-replayable. This is a narrower and truthful assertion than treating every failed factory as an already-completed tuple.

## Shared detached directory order and limits

Within one row, directory preparation iterates both geometry wrappers before releasing common G (`particles_storage_retire.cpp:649-669`). When both reference the same detached Surface, the first shared wrapper returns only its authenticated metadata share; the remaining last wrapper marks the complete original roots, then G may release. A refused mark leaves its cursor intact. Across rows/groups, a shared wrapper can release its own physical shares after returning metadata while a later wrapper retains the complete directory; the final wrapper must mark the actual roots before its own group release. The reviewed source does not require speculative reordering of pipeline/group service to close the demonstrated double-charge.

The parent reports six continuation cases GREEN6/8399 and combined57/18661 exit0. Those are parent execution results, not runs performed here. Modes3/4/5 exercise last-holder detached success/close/factory-failure; they do not by themselves establish every concurrent/shared-holder interleaving, real GPU execution, or production public issuer authority. This review makes no broader runtime claim.
