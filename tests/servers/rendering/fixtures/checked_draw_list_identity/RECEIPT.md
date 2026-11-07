# Checked draw list identity — 2026-11-07

Two draw ID defects are covered here: `_record_source_draw_indirect` compared a public begin ID against the bare draw enum, and three checked leaves (render pipeline bind, indirect draw, enable scissor) omitted the encoded list check. The source wrapper now compares against `int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT`. The three leaf guards apply when `p_capture` is true, before list state, graph or transfer changes. Existing thread and recording-error precedence is retained. Ordinary public wrappers continue passing `false`.

`SOURCE_CHANGE.patch` is the exact four-file draw diff against `9558cdc89578886e081b59b472fc476f3202467d`: `servers/rendering/rendering_device.cpp`, `tests/servers/rendering/test_checked_draw_bind.cpp`, `tests/servers/rendering/test_record_source.cpp`, and `tests/servers/rendering/test_external_output_contract.cpp`. Base/current file hashes are in `RECEIPT.json`.

The green executable was built with both this draw diff and a metadata fix. The three metadata files (`core/extension/gdextension_production_request.cpp`, its header, and `tests/core/extension/test_production_request.cpp`) are listed and hashed as `compiled_overlay`; their edits are outside this draw patch. Therefore the green binary is not a draw-only build. Its recorded size is 156,730,024 bytes and SHA-256 is `d75aa4372d76d47aa98640cb39a7f5db55da86b6f9139daf480372ab10e11c79`.

## Recorded validation

- Red: root executed the two new draw cases against the original production guards. Exit 1; 2 cases failed; 243 assertions, 142 passed and 101 failed. `checked-draw-red.log` records the exact failures. For example, checked-bind line 207 accepted an invalid ID (`OK` instead of `ERR_INVALID_PARAMETER`); record-source line 615 accepted the bare enum; line 655 rejected the genuine begin ID; line 628 showed transfer operation 9 consumed where zero was expected. These line numbers identify the red log's source version.
- Green build: root executed the complete command in `COMMANDS.json`, exit 0. The build log is retained.
- Focused green: 66 cases and 6,623 assertions passed, exit 0. The log contains three expected foreign-camera-token diagnostics from negative camera tests.
- Combined green: 179 cases and 80,359 assertions passed, exit 0. The log contains six expected diagnostics: three pipeline-creation refusals and three foreign-camera-token refusals.

The red run used the initial draw fixtures and original production guards. The green build restores the draw guards and includes the metadata fix and its additional negative coverage. This is recorded root execution evidence, not a preserved isolated red executable: no red binary, byte count, hash or complete red-source snapshot is claimed. Green execution commands and binary metadata are copied verbatim in `checked-draw-metadata-green-runs.json`; `COMMANDS.json` also includes the draw red command and root-confirmed build command. No test or build was rerun while preparing this receipt.

## Behavior and limits

Both draw success fixtures call the real public `framebuffer_create_empty` and `draw_list_begin` path and pass its returned ID. The checked-leaf case rejects a bare draw enum, bare compute enum, encoded compute list and encoded draw ID plus one without captured effects. Record-source coverage retains bound identity, actual graph handles, exact transfer pairs, descriptor-prefix refusal, indexed draws and zero draw count. The adjacent external-output fixture retains its controlled graph begin and now supplies the matching encoded ID, preserving its span, preparation and transfer assertions.

`review.txt` preserves the independent cross-review by `/root/production_raster_frame_impl` (user-designated Astra), relayed by root. It confirms guard placement, ordinary wrapper behavior, begin encoding and fixture cleanup. These are controlled CPU/source/graph tests. They do not establish physical GPU qualification, output issuer or packet authority, or end-to-end particle acceptance.
