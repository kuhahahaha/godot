# Independent Production raster preparation

Implementation slice: request-owned depth/shader/framebuffer/pipeline preparation and physical retirement. Full editor build and CPU runtime verification passed on 2026-11-06; see RESOURCE_PREPARATION_RECEIPT.md. This does not claim procedural drawing or real-GPU acceptance.

## API and original ownership

`device_session_production_raster_prepare_v1(request, prepare, shader_binary, bytes, out_report)` accepts `GDExtensionProductionRasterPrepareV1`: containing ABI header, nonzero `create_attempt`, nonzero `width`/`height`, `shader_bytes == bytes`, and zero `reserved`. The binary is the RD driver's supported compiled shader-container format. It is copied into the original request's prequoted retained payload, never retained as a caller pointer.

`device_session_production_raster_prepare_report_v1(request, out_report)` reads the retained report without scheduling, consuming a control, calling RD, or admitting new work. No accepted preparation returns ERR_DOES_NOT_EXIST. Query failure leaves a validated output zeroed.

Raster reserve now requires caller shader/pipeline/framebuffer serials to be zero. Core assigns physical identities; callers cannot predict or mint them. It requires one action, four resource slots, four return cells and at least eight graph memberships. Preparation additionally requires the complete shader bytes to fit the existing payload slice. No arena growth occurs.

The four original cells are slot 0 framebuffer, slot 1 depth, slot 2 shader, slot 3 pipeline. All share one Core-generated `owned_serial`/physical child representing this raster owner. Each has its own Core-generated physical serial and retire serial. Their physical parent field is the accepted Production attachment serial. No qualification bridge, ticket, exported qualification backing or bare RID supplies authority. Existing RD functions with historical `_qualification_` names are used only as mechanical constructors with these exact request-owned cells.

## FIFO and immutable attempt

The accepted origin is `(same request, create_attempt, width, height, exact shader bytes)`. A different attempt, dimension, extent or byte rejects without replacing it. A fresh attempt requires a fresh request; creation failure cannot transparently replay allocation in the original request.

One original `RASTER_CREATE` fixed control owns preparation. It uses the existing FIFO and holds the request arena through native work. Repeated calls may publish the same still-PENDING control after a known FIFO refusal, or consume its already RETURNED result. They do not allocate a second control or rerun a returned native attempt. Read-only report never performs either action. A returned preflight error is terminal for this preparation attempt too.

All four native free-event credits and RID reservations are obtained before the first native allocation. Shader-container bytes are materialized in an engine-local temporary vector before entering native work; the original retained bytes remain in the accepted arena. Native allocation is ordered depth → shader → framebuffer → pipeline. The depth texture uses D32_SFLOAT, one layer/sample, and sampling plus depth-attachment usage. Pipeline creation uses the original restricted depth raster profile. Ordinary native validation remains authoritative.

The Registry lock is released around each original factory while capture retention and RD ownership remain live. Each step rechecks the original admission/device/finalization state before admitting the next allocation. Public cancel/close retain their existing OWNER_CONSUMED gate: during EXECUTING they return ERR_BUSY and do not mutate cancellation state. No new concurrent cancellation permission is introduced.

## Report interpretation

`GDExtensionProductionRasterPrepareReportV1` contains `request_serial`, retained original `control_serial`, `create_attempt`, current request `revision`, `known_mask`, `returned_mask`, `phase`, zero `reserved`, retained `result`, and framebuffer/depth/shader/pipeline physical serials.

Mask bit i refers to resource slot i. `known_mask` records objects actually created; it remains set after physical return. `returned_mask` records completion of their original physical retirement and is a subset of known_mask. Uncreated objects are not reported as returned objects. Physical serials are published for actual created objects only. Output contains no caller-usable RID authorization.

Before execution, phase is 0 and result ERR_BUSY. Executing phases are 1 depth, 2 shader, 3 framebuffer, 4 pipeline; result remains ERR_BUSY until the exact control returns. Phase 5 means all four constructors completed and the action was prepared. A successful preparation requires phase 5, known_mask 15, returned_mask 0 and successful original creation return; neither API admission nor phase alone supplies new recording authority. Record scheduling still performs its original live admission and freeze checks. Subsequent cancellation/retirement does not rewrite the historical creation result; current returned_mask and request lifecycle remain independently relevant.

The result/control identity survive later OBSERVE controls. Repeating the same preparation after its result was consumed reads its retained outcome; it cannot recreate already returned resources. A later RECORD uses the exact accepted physical callback and checks actual owned cells/serials. The native framebuffer entry accepts only its original exact Production resource-permission join; qualification recording permission is not enabled.

## Partial failures and retirement

Any constructor failure retains all created prefix resources and their exact return cells. Only never-created RID reservations and their free-event reservations are discarded. No failed constructor causes all four resources to be treated as absent. Exact no-RD/dispatch-preflight rejection also finishes the original attempt and releases only its never-created return obligations.

Failed preparation is eligible for the original bounded OBSERVE retirement. Successful preparation becomes eligible after owner cancellation or completed record action. Preparation never marks business append: it sets native-effect history only. Allocations and cleanup therefore retain physical obligations while a later exact empty RECORD can still report TERMINAL_NO_APPEND. Neither flag resets the original Runtime's AppendStarted/once handoff.

Retirement keeps the existing resource reverse edges until recording/source obligations permit cleanup. It enqueues framebuffer and pipeline first. Their dependency counters on depth and shader decrement only after actual native/cache free, not at RID invalidation or enqueue. Parent resources wait for zero dependent children. Shader retirement checks `shader_retire_ready_nonblocking`; a ready observation may be retained across bounded owner turns, and the original RD free/disposal paths retain their own readiness checks. Foreign reverse edges are never removed.

Native maintenance admits disposal kinds 0 (render pipeline), 5 (shader), 7 (framebuffer) and 8 (texture) only through exact existing request-owned physical-cell pointers, alongside already supported original disposal memberships. Shared same-RD maintenance may advance another live Production request's original return cell; receipts remain attributed to that original owner. Qualification bridge cells, copied identities and foreign queue prefixes remain rejected. Unsupported raytracing/acceleration kinds stay blocked. The same rule applies to fresh and resumed disposal cursors.

Only the original driver/cache free updates the physical return event. OBSERVE then marks the resource returned and clears its accepted pending return obligation. Cancel is not a physical return; release_v2 remains blocked until all actual obligations and accepted controls return.

## Independent verification boundaries

Required component branches: zero caller serials; insufficient payload/return capacity without admission; exact repeated request versus changed bytes; each of the four real constructor failures with correct retained prefix; public cancel refusal while EXECUTING; no new native allocation after terminal attempt; successful prepare then owner cancel; shader not-ready and later readiness; framebuffer→depth and pipeline→shader actual-free ordering; bounded repeated OBSERVE; final release after all four original returns; foreign disposal prefix remains Busy; native history true and append history false throughout preparation-only work.

A separate record-window join can verify prepared owned resources reach actual checked begin/end and their graph capture. Visible depth drawing, shader output, readback correctness and GPU completion are separate acceptance boundaries and are not established by the preparation fixture or a mock driver.
