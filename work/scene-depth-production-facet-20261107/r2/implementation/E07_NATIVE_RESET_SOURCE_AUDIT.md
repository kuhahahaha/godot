# E07 native reset source audit

Date: 2026-11-07
Scope: read-only audit of the current Godot production path for the E07 request. No production or test source was changed.

## Finding

The current tree has no E07 `OrderedReset`, `E07LifecycleControlTerminalRef`, or lifecycle reset caller. `rg` over `core`, `servers`, `modules`, `scene`, and `tests` finds no `OrderedReset`, `E07Lifecycle`, `require_source`, or `RuntimeLifecycleObligation` symbol. The only reset-like production match is `gdextension_production_output.cpp:61` (`common.history_reset <= 10`), which is camera/history output policy, not a native reset. `gdextension_production_request.cpp:2599-2600` resets metadata arrays during request cleanup; it is arena cleanup, not an ordered native reset.

Therefore the requested E07 source is currently missing. Existing production machinery can provide dependencies and evidence, but it does not implement the required reset ordering or terminal source contract.

## Existing original production chain

* Action grammar and reservation are in `core/extension/gdextension_production_request.cpp:580-635`. `GDExtensionProductionActionV2.action_kind` accepts DISPATCH, COPY, UPLOAD and READBACK; contract v2 additionally accepts `GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1`. There is no RESET action kind. Native output has no physical unit/readback slot and is constrained to a continuing list.
* `Registry::production_action_locked` is declared/used at `gdextension_production_request.cpp:858-873`; append entry points for dispatch/copy/upload/readback are at approximately lines 1062, 1123, 1175 and 1216. They call `production_dispatch_closure_locked` or the corresponding record path; none calls a reset operation.
* `Registry::production_dispatch_closure_locked` (`gdextension_production_action.cpp:174`) validates the real pipeline, shader, uniform sets, aliases, and sealed use windows before entering the mechanical record tail. The tail at `gdextension_production_action.cpp:640-734` calls the native RD source methods `_record_source_dispatch`, `_record_source_buffer_copy`, `_record_source_buffer_upload`, or `_buffer_get_data_async` for readback, then closes a list when required and captures the returned checked append.
* RD source methods are declared in `servers/rendering/rendering_device.h:340` and implemented in `servers/rendering/rendering_device.cpp:16092` onward. `_record_source_dispatch` first binds `OriginalOperationIdentity`, checks render-thread/list/pipeline/set ownership and state, then records the actual command. This is an append source, not a reset source.
* `OriginalOperationIdentity` is the existing immutable join in `core/extension/gdextension_record_source.h:65-80`: owner request nonce, library generation, qualification session serial, record generation, and canonical record range. `production_append_v2` constructs it at `gdextension_production_window.cpp:1134` and calls `production_record_effect_locked`.
* `production_record_effect_locked` (`gdextension_production_action.cpp:497`) stores each real checked graph append in `request.graphs`, preserving action ordinal, canonical suboperation, source recipe substep, graph handle and command slot. `production_capture_graph_locked` (`gdextension_production_action.cpp:261`) rejects unbound or non-recorded handles and stores the frame/qualification epoch.
* At frame issue, `RenderingDevice::_production_retain_frame_sources` and transfer-worker paths call `Registry::production_capture_issue_locked` (`servers/rendering/rendering_device.cpp:9495-9557`). That function accepts only actual queue report calls with serial, queue index, completion serial and enabled completion capability. It creates bounded `request.natives` rows, deduplicates by native serial tuple, preserves the graph membership parent, issue/completion serials and native issue errors, and marks pending/effects on failed or incomplete capture.
* Native maintenance is `Registry::production_poll_native_locked` (`gdextension_production_request.cpp:1462`) using `poll_native_source` from `gdextension_record_source.h:108-110`, with finite per-turn pass/receipt caps. Release is `production_release_locked` (`gdextension_production_request.cpp:2409`), which refuses while captures/effects/graph/readback/native references remain and calls `release_native_source` only after completion and retention are proven. This is cleanup of an existing source, not a reset producer.

## Real callers and missing caller

The only current production caller census is the checked append path (`production_append_v2` and its dispatch/copy/upload/readback helpers), frame issue capture, native observation, and release. Qualification/system scalar have separate `_record_source_dispatch` callers, but those are their own original operation families (`gdextension_qualification_ticket.cpp:351,374` and `gdextension_system_scalar.cpp:167`) and do not provide E07 lifecycle ownership. No genuine production caller currently records a reset after preserved prefixes and outstanding sources. No existing terminal completion/cleanup callback is wired to an OrderedReset source.

## Minimal D-owned seam and finite write set

D should add only the original native owner seam once C/B provide exact identities and the registered recipe:

1. Add a private reset action/source declaration to the original production owner (the existing action family), with finite reservation fields for reset payload, source dependencies, completion and cleanup. Do not create a second ledger, issuer, runner, or receipt table.
2. Add one checked RD reset record method adjacent to `_record_source_dispatch` that accepts the original `OriginalOperationIdentity` plus the owner-issued reset mapping/source token. It must reject stale owner/generation/range, wrong render thread, open/invalid list state, and missing preserved-prefix/source proof before recording the native reset.
3. Extend the existing action dispatch/append switch and source capture mapping so the reset row is linked to the same action, graph membership and native source tuple. Keep `production_capture_issue_locked`, `production_poll_native_locked`, and `production_release_locked` as the completion/cleanup path; do not reinterpret initialization clear or `Cancel` as completion.
4. Add only the corresponding source registration/build files and positive/negative production fixtures after C/B return exact recipe, mapping, native reservation and terminal identities. The fixture must exercise real append -> issue/source capture -> completion -> cleanup -> reset ordering, plus Busy/no-effect cases and finite-capacity exhaustion.

This is a bounded seam proposal, not an implementation authorization: the exact action kind/struct fields and reset method name must come from the original C03/C04/C06 owner package.

## Exact identities required from C/B

Before D writes the seam, C must provide the exact Closure source identity and frozen frontiers: CPU/channel/GPU event birth frontier (including last births), closure cut/count-zero evidence, and cleanup obligations. B must provide the exact OrderedReset recipe, source/mapping/record/completion identity, native reservation/append/readback/terminal body and the owner-issued generation/content/frontier binding. Both must identify the existing `AcceptedStepState`/`FullSettlementPolicy` cells to reuse. D will consume those identities through the existing original operation/source mapping; it must not invent a private fallback or use an old snapshot bool.

## Verification status

This audit is source-only. No build, renderer run, GPU run, test mutation, commit, or frozen public5 R2 source change was performed.
