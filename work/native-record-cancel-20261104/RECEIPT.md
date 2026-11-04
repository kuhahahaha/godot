# Native cancellation after record admission — 2026-11-04

The new standalone native mode 3 exercises cancellation after the original V2 record has accepted three readbacks and before any owned result is reported. On the retained no-trace Metal main-RD host, cancellation closed further upload/record, all original admitted readback facts drained, result copying stayed refused without modifying its destination, and physical resource retirement completed. No Godot production change was needed.

## Run and observed behavior

```sh
python3 work/native-record-cancel-20261104/run.py
```

Final verification: `results/20261104-081742/results.json`. The container process and cancellation process exited 0. The deliberate omit-cancel control exited 180 at exactly `after_record_cancel_closes_upload`; this is the expected control failure, not a passing engine process. All 13 runner checks passed. Two standalone native DSOs compiled successfully; no Godot or ANGE build was started. Logs retain full stdout/stderr, and JSON retains commands, executable metadata, actual linkage, input/DSO/container/log hashes and exit codes. Host and input hashes were unchanged.

- Host executable SHA-256: `887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392`.
- Controlled-build metadata SHA-256: `c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a`.
- Real backend: Metal 4.0, Forward Mobile, Apple M3 Pro, ordinary main RD with safe render thread.
- Record-return snapshot: `state=2 phase=2 owned=0 readbacks=3`.
- Cancelled terminal: `state=7 phase=6 admitted_readbacks=3 returned_bytes=44 owned=0`.
- Returned original readback facts: backing serials 13/14/15, offsets 0, sizes 24/16/4, native issue serial 12, completion serial 12, queue 1. Each has issue 1/completion 2/error 0; exact ranges and distinct original asynchronous request serials are asserted in C++.
- Six distinct typed physical events (sequences 3–8) completed with exact object/RID/source checks, nonzero frame/dependency, refused stale ACK, and single current-revision ACK. Native units and original storage then retired and acknowledged, followed by bridge/session/context/CodeHolder/guard cleanup and DSO unload.

The retained executable is the previously built no-trace baseline, not a rebuild of current dirty Godot sources. The driver derives from the original typed control copied into `runtime/`; mode 3 adds record-before-cancel, completed-original-range validation, result suppression and destination preservation checks. Source files needed to build this variant and generate its containers accompany this receipt. The exact header pin remains part of the runtime.

## Test effectiveness and review

`OMIT_AFTER_RECORD_CANCEL` removes only the two owner cancellation calls from the new native control. On the same original host and shader inputs, the retained upload replay remains accepted, so the precise closed-upload assertion fails with exit 180. The normal control closes upload/record and completes the entire drain/retirement sequence. This is a mutation of the calling sequence, not a mutation of production Godot code.

Independent reviewer `/root/review_record_cancel_native` checked the runner, C++ mode and actual logs. The reviewer found that `ERR_BUSY` plus zero reported bytes did not establish destination preservation. The final C++ now fills all eleven destination words with `0xdeadbeef` immediately before the cancelled copy and verifies every word afterward. The final run above includes that assertion. Review also limited the evidence to admitted-readback cancellation; those limits are preserved below.

## Integration implication and limits

The supported sequence is original QRT record return → original ticket cancel (replay allowed) → refuse new upload/record → observe and retain all original successful native facts → refuse copy with zero written bytes and untouched destination → release settled ticket → typed resource retire/observe/current-revision ACK → native unit/storage teardown and bridge/session close. The unchanged production stop linearization is in `core/extension/gdextension_qualification_ticket.cpp::qualification_cancel`; `qualification_refresh` requires original source return before marking the cancelled ticket physically retired.

This proves actual Metal backend execution and the original Godot native cancellation boundary. It does not establish an authentic ANGE caller association, product close/cancel behavior, live particle pixels, cancellation after only some readbacks complete, or that GPU work was still executing at the cancellation instant. The state 2 snapshot is a logical record-return observation. All three readbacks had been admitted before cancel; no timing injection or partial-readback control is claimed. Exact result bytes are deliberately unavailable after cancellation; the separate positive native chain verifies those bytes.
