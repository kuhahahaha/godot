# Vulkan native qualification refusal — 2026-11-04

The Vulkan attempt did not reach record or cancellation. Actual Vulkan native container generation passed, and original storage/native-unit creation succeeded. V2 prepare was refused with `ERR_UNAVAILABLE` (2). The runner failed correctly; this is not Vulkan cancellation acceptance.

## Exact attempt

```sh
python3 work/native-record-cancel-vulkan-20261104/run.py
```

Results: `results/20261104-082012/results.json`. The runner is copied from commit `2fcde10e60` with only its requested backend and startup checks changed to Vulkan. Runtime C++/includes/header/GLSL/project sources were extracted unchanged from that commit into the isolated `runtime/` directory. Their hashes are retained in the result. Reconstruct those inputs from `2fcde10e60:work/native-record-cancel-20261104/runtime/` to repeat this attempt; use the Vulkan runner in this directory. The original Metal runtime and its evidence were not modified.

Host: the retained no-trace editor SHA-256 `887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392`, controlled metadata `c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a`. Actual startup: Vulkan 1.2.357 / Forward Mobile / Apple M3 Pro. No engine or ANGE build was started.

The container process exited 0 and generated Capture/Body containers of 536/756 bytes. The first native process exited 91 at `prepare_v2 error=2`, before the omit-cancel control's intended assertion. Its target check is false and the overall runner exits 1. No cancellation process was run after that prerequisite failed.

## Original refusal attribution

One additional invocation of the same native process enabled existing `GODOT_QUALIFICATION_CAPACITY_TRACE=1` diagnostics. Exact command, host/source/DSO hashes before and after, and exit status are in `results/20261104-082012/capacity-trace.json`; raw output is in `capacity-trace.log`. It again exited 91; all pinned bytes were unchanged.

All four original upload destinations passed validation. The trace then returned:

```text
QCAP native current=0 graphs=10 workers=11 frames=11 main_slots=11 worker_slots=121 total_slots=132 generic_bound=0 profile_bound=0 capacity=4096
PHS1_FIXTURE_FATAL tag=one where=prepare_v2 error=2
```

Current source agrees with that attribution: `core/extension/gdextension_qualification_ticket.cpp:46` computes the bounded original submission profile, rejects a zero backend bound as unavailable, and prepare calls this gate before creating the ticket (`:727`). `drivers/vulkan/rendering_device_driver_vulkan.h:414` supplies only the narrower `record_native_call_bound` profile; it explicitly retains the existing unavailable qualification cap. `servers/rendering/rendering_device_driver.h:556` therefore supplies qualification bound zero. Native-call observation support alone is insufficient to admit this full qualification profile.

## Next integration boundary

Keep the Metal driver as the verified full typed qualification control. Vulkan would require a separately justified complete qualification submission bound and its corresponding real native mapping/retention proof before this driver could reach record/cancel. The narrower original record profile is a separate possible integration lane. This attempt does not justify raising a cap, changing fallback behavior, bypassing the gate, or treating fail-closed refusal as completion/readback/physical retirement. It does not affect the accepted Metal result in `2fcde10e60`.
