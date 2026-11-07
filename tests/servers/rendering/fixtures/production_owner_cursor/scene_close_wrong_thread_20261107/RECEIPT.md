# Original Scene unregister wrong-thread coverage

2026-11-07. Added regression coverage in `tests/core/extension/test_production_request.cpp`; no production bug fix, ABI change, or new issuer. The existing installed Scene unregister port already returns Busy on foreign-thread destruction and permits its original owner to complete close.

The new THREADS_ENABLED case invokes that actual installed port on an auxiliary Godot Thread before any RECORD is queued. It distinguishes the intended holder gate from mailbox/delivery Busy: registration closes and cancellation persists, while release_requested/cell_released/holder_closed remain false and no callback is freed. Owner retry releases the Registry-held cell; dropping the final original Callable runs its actual free callback exactly once on the captured owner/destruction thread. Final unregister and its cached retry agree byte-for-byte; original requests cancel/release and owner cleanup complete. No callback deliveries, draw or GPU work occur in this branch.

## Actual local evidence

| Run | Executed cases | Assertions | Exit | Captured output |
|---|---:|---:|---:|---|
| FOCUSED |1|185|0|empty|
| NEIGHBORS_COMPLETE |5|1102|0|empty|
| COMBINED |217|90839|0|495B; three expected negative render-pipeline diagnostics|
| Earlier narrow NEIGHBORS |2|479|0|empty; intentionally retained as narrower evidence|

Counts are independently parsed from the actual XML, excluding skipped cases; assertion and case failures are0. The complete neighbor set contains early-hook delivery, close-waiting, close-FIFO-full, new wrong-thread and original TypedABI Callable-retirement cases. The older filter `*[NativeScenePhase]*` matched only the exact bracketed early-hook tag plus the separate relay case; it did not execute the close cases. It is not relabeled as complete.

The parent reran NEIGHBORS_COMPLETE solely to retain its exact command: exit0/0.37s,5/1102. Its current XML and command are that rerun. FOCUSED1/185 and COMBINED217/90839 remain their original runs (2.34s/4.1s). Exact argv, exits and counts are in RESULT.json; the complete rerun's original record is NEIGHBORS_COMPLETE_COMMAND.json. All commands ran from this continuation worktree using `bin/godot.macos.editor.arm64 --headless --test`; no additional runs were performed by the receipt auditor.

Current observed host:157540936B, SHA256 `f19eb511955cb68d0dced014b6858290540ae9fc0b54a07d15bc903ba8e2673b`, matching the actual controlled-build metadata copied unchanged to HOST_CONTROLLED_BUILD.json (530B). Metadata is artifact identity, not a substitute for build logs. Current test-source hash at receipt preparation: `6cf1a0cadf968fa02d5c0266560bef97a5a56b1cae3d36c46ad3c14d6a5673c4`. Existing unrelated CI formatting/mode work is a separate change; this receipt does not claim the whole workspace or GPU was requalified.

## Evidence preservation and staging

Original *.log files remain untouched and ignored under the existing rule. Each *_OUTPUT.txt is an exact byte copy, including the empty logs and the495B combined diagnostic stream; these copies preserve output without force-adding ignored logs or widening ignore rules. SHA256SUMS.txt covers the result, exact complete command, four XML reports, four output copies, host metadata and this receipt. EVIDENCE_MANIFEST.json records source-log/copy byte equality, hashes and the exact stage list. No raw evidence is replaced with a summary.

Stage only the test source plus the evidence files listed in EVIDENCE_MANIFEST.json. AUDIT.md is the earlier design note, not an executed-run receipt; it is intentionally outside this package. This evidence qualifies an auxiliary-thread holder lifetime regression on the CPU/dummy test driver with non-threaded RenderingServer. It does not qualify threaded RenderingServer, Metal/Vulkan draws, device loss, K06 HeightFrame, E09 operation issuance or Full P01/production Sprite execution.

Archive location: `tests/servers/rendering/fixtures/production_owner_cursor/scene_close_wrong_thread_20261107/`. This package is copied from the original work evidence; the commands and XML output destinations retain the actual work paths used at execution. No tests were rerun from this archive directory. Only archive-local receipt/manifest/checksum identities are refreshed; original work files remain preserved.
