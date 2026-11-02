# D native input handoff — 2026-11-02

Status: FORMAL_PRODUCTION_HANDOFF_NOT_READY. This is an intake checklist for the existing R02 acceptance table, not a new API or runtime receipt.

## A supplies one reproducible production input package

The minimum executable owner handoff is a production caller that invokes `HostControlService::prepare_system` with its authentic `GpuPrepareRequest`, then invokes `request_method_qualification` with the retained `MethodQualificationLink`, recipe, demand and compile target from that same issued request. The caller must keep the original coordinator and qualification scope alive through E09. Its machine-readable correlation report must include the issuer/caller name, immutable source/request identity, source role/schema/decoder, bridge and ticket `registry_instance`/`handle_serial`, every prepare identity field, upload plan/RID/backing/range and result byte expectation, every record/readback range, same-ticket report revision/state/owned_size, issue/completion and readback serials, exact copy rc/count, ticket cancel/release rc, typed child retirement/ack rc, bridge close/unload rc, and final process status. The raw readback bytes must be a separate declared output file associated with the same ticket and report revision. Missing fields stay pending in the acceptance table; the capture controller does not fill them from fixtures.

1. Pin the Godot source revision and dirty source hashes, editor absolute path and SHA-256, generated SDK header hash, ANGE source revision and dirty source hashes, DSO absolute path and SHA-256, renderer/device, full launch command, project and input paths. Historical fixture DSO files do not identify the production pair.
2. Name the original production issuer/caller and its immutable request/source identity. Supply the original System/instance admission projection and matching Method qualification link/demand consumed by HostControlService::prepare_system and request_method_qualification. Retain the coordinator, source and qualification scope through E09. A copied correlation key does not establish this ownership.
3. Capture original bridge and ticket registry_instance/handle_serial; the complete prepare tuple, each upload plan and its payload digest; every record/readback range tuple. Field definitions and source consumption sites are in R02_CORRELATION_ACCEPTANCE_TABLE.md. Include source role, schema and result decoder supplied by the owner.
4. Provide a deterministic expected result from that request, including byte length, raw expected bytes or an independently reproducible byte oracle, range split and digest. Keep the owner identity associated with the expectation.
5. Provide the same-owner cancellation/failure trigger and expected settlement outcome separately from the successful run. Keep already accepted native work and physical retirement observable.

## D executes and captures

Use the fixed pair to capture request admission, prepare/upload, prepared record/submit, native completion facts, exact owner copy and release/drain in one request lineage. Capture each API return, ticket report revision/state/owned_size, readback request serial and source range, exact copied bytes and digest, child retirement/acknowledgement, bridge close/unload and process exit status. Preserve full stdout/stderr and timeout outcome. Main RD submission follows the render loop; CPU queue sync is not a GPU completion observation.

For the successful copy, state must be 8, physical retirement observed, revision current, capacity exactly owned_size, and all readback identities complete. For release, no accepted RT/frame references, recording or queued work may remain; physical retirement and original reverse edges must be valid. Capture the intermediate busy responses when applicable.

For the cancellation run, qualification_cancel is the owner stop linearization point and closes admission while admitted work settles. Observe settlement and retirement before release; a successful cancel alone does not establish physical return or successful close/drain.

## Acceptance boundary

Missing issuer, pair, byte oracle, same-ticket facts, or return/drain evidence leaves the corresponding six-link gate pending. Reject evidence spliced from another ticket or fixture. Existing native fixture evidence remains valid within its recorded scope. This checklist makes no production, Ready, G1 or whole-particle-system completion claim.

## Source check at intake

Godot HEAD: 1fe430f0b9e6e2dd154aedde74a46f214c13bbeb (dirty). Current ticket implementation locations: qualification_copy 524, qualification_release 557, qualification_cancel 849, qualification_record_prepared_v2 909 in core/extension/gdextension_qualification_ticket.cpp. Existing A service declarations are at host_attachment.h:384,393 and definitions at host_qualification_service.cpp:73,123. These locations were read directly; no joint production run was executed for this checklist.
