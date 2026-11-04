# Native bridge close with a live System scalar ticket — 2026-11-04

Both Metal and Vulkan retained the original System scalar ticket after an early native bridge close. Close returned busy while children remained, refused a new allocation without publishing a backing, and preserved the original owner's cancellation/observation/retirement path. After ticket, unit and storage acknowledgement, final bridge close succeeded and the DSO unloaded. This is the original Godot bridge boundary; it does not certify the ANGE Host wrapper's caller-context lifetime.

## Reproduce and results

```sh
python3 work/native-system-scalar-early-close-20261104/run.py --driver metal
python3 work/native-system-scalar-early-close-20261104/run.py --driver vulkan
```

| Actual backend | Result directory | Container / omit-close control / close-then-cancel exits | Checks |
| --- | --- | --- | --- |
| Metal 4.0 / Forward Mobile / Apple M3 Pro | `results/20261104-083348-metal` | 0 / 180 / 0 | 11/11 |
| Vulkan 1.2.357 / Forward Mobile / Apple M3 Pro | `results/20261104-083403-vulkan` | 0 / 180 / 0 | 11/11 |

The runner retains complete commands, stdout/stderr, actual input snapshots and hashes, container digest, DSO build commands/digests and log hashes in each result directory. Current source/snapshot/DSO/log identities were rechecked after execution. Host and input bytes stayed unchanged. Only small standalone DSOs were compiled; no engine or ANGE build was started.

The existing no-trace executable SHA-256 is `887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392`; controlled metadata SHA-256 is `c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a`. This evidence identifies that executable, without asserting a new matched build of all current dirty sources.

## Actual native sequence

1. Bind the original SceneReady main-device session and bridge, create the original E05 output and native unit, and invoke System record on the real render thread. The original ticket is 12; record returns logical state 2 and owned size 0.
2. Call `device_session_qualification_close_v1` twice. Both return `ERR_BUSY` (44), reason 16, while retaining live children. The driver then requests a distinct storage allocation through the original QRT. Admission returns busy and publishes no RID, backing handle or accepted state.
3. Call original `device_session_system_scalar_cancel_v1` and replay it. Both succeed on the retained original ticket. Continue original observe/facts until state 7/phase 6 with owned size 0. Body and the original four-byte readback complete successfully; Metal retains four facts, Vulkan three. Native issue/completion serials are Metal 11/12 and Vulkan 7, with no possible/unknown outcome.
4. Copy at the observed revision returns busy, zero bytes and an unchanged complete `0xdeadbeef` destination. Ticket release then succeeds once. The native unit's shader/pipeline free-event fields and storage physical report are checked before acknowledgement. Final bridge/session/context close and guard cleanup complete, with process exit 0 and DSO unload.

The unchanged native behavior is explained by `Registry::qualification_close`: it marks the bridge closing and tickets admission-closed, then returns busy while children remain. `qualification_bridge_locked(..., false, false)` keeps original owner operations available; `system_scalar_cancel` uses that owner observation path. Cleanup QRT operations remain available for original retirement. The driver adds no alternative owner, context or ticket.

## Control and evidence limits

`OMIT_EARLY_BRIDGE_CLOSE` omits only the two early close calls. The distinct new allocation is then accepted, and the control fails precisely at `bridge_close_refuses_admission` with exit 180. Its raw log is retained; arbitrary failures are not treated as valid controls.

This is a local native control with fixed test identities. It is a concrete comparison for a Host wrapper that invalidates its caller context too early; it does not prove that wrapper is fixed or that the ordinary World request is connected. It does not prove partial-readback cancellation or GPU-in-flight timing. Internal set return remains supported by successful ticket release plus the existing production `free_event` gate, not by a directly exposed internal event. The positive scalar output bytes are covered by `e763641dea`; this cancelled path deliberately exposes no result bytes.

Independent reviewer `/root/system_native_close_review` verified both final runs, source/snapshot/DSO/log identities, the original owner/cleanup paths and the exact omit-close failure. No blocking finding remained; native-versus-Host scope limits above were retained.
