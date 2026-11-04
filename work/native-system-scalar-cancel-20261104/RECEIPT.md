# System scalar cancellation after native record — 2026-11-04

The original System scalar cancel API was exercised after real render-thread record returned a ticket with logical state 2 and no owned result. Both Metal and Vulkan completed original native facts, suppressed result copying, retired the ticket and original unit/storage, and finished guard cleanup. This adds cancellation coverage to the separate positive System scalar driver in `e763641dea`; no production source changed.

## Commands and results

```sh
python3 work/native-system-scalar-cancel-20261104/run.py --driver metal
python3 work/native-system-scalar-cancel-20261104/run.py --driver vulkan
```

| Actual backend | Result directory | Container / omit-cancel control / normal-cancel exits | Checks |
| --- | --- | --- | --- |
| Metal 4.0 / Forward Mobile / Apple M3 Pro | `results/20261104-083121-metal` | 0 / 180 / 0 | 11/11 |
| Vulkan 1.2.357 / Forward Mobile / Apple M3 Pro | `results/20261104-083136-vulkan` | 0 / 180 / 0 | 11/11 |

Each result directory retains the commands, complete stdout/stderr, exact executable/metadata identities, actual source snapshots, generated-container digest, DSO build commands/digests and log digests. Source/snapshot/DSO/log hashes were rechecked after both runs. Input and host bytes were unchanged. Only small standalone DSOs were compiled; no engine or ANGE build was started.

The pinned host is the existing no-trace editor SHA-256 `887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392`, controlled metadata SHA-256 `c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a`. Its current dirty source tree is not asserted to be a new matched build.

## Original native observations

The driver preserves the original main-RD bridge, CodeHolder, E05 output and native unit. It invokes System record through an actual public render-thread Callable, verifies `is_on_render_thread`, waits for the callback's atomic return, then calls the original owner-thread `device_session_system_scalar_cancel_v1` twice on the same ticket. Both calls succeed, including the replay.

Both normal cases log `record_state=2 owned=0 ticket=12` before cancellation and reach `state=7 phase=6 returned_bytes=4 owned=0 copied=0 sentinel_unchanged=1`. Original Body and readback facts are retained and successfully complete: Metal has four total facts (including native serials 11/12), Vulkan has three (native serial 7). The readback source remains original backing 13, offset 0, size 4. Every retained fact has issue 1/completion 2/error 0, nonzero issue/completion serial and queue, with no possible/unknown native count.

After observing the cancelled terminal, copy must return `ERR_BUSY`, report zero written bytes and preserve the complete `0xdeadbeef` destination word. The driver then releases the ticket once, checks original unit shader/pipeline free-event fields and original storage physical report, acknowledges them and closes bridge/session/context before guard cleanup and unload.

Internal set physical return is supported by successful ticket release combined with the existing production `free_event` release gate. The driver does not directly expose that internal event or independently prove that deleting the gate would fail.

## Control and independent review

The `OMIT_SCALAR_CANCEL` control removes only the two cancellation calls from the calling sequence. On the same backend and shader, the ticket instead reaches a normal completed result and fails exactly at `scalar_cancel_terminal_identity` with exit 180. The runner requires that precise failure and keeps its raw log. No arbitrary engine failure is treated as a valid control.

Reviewer `/root/system_scalar_cancel_review` independently checked production API semantics, both final runs and source/snapshot/DSO/log identities. All 11 checks per backend matched, including the exact omit-cancel failure and successful cleanup. No blocking issue remained.

This is a local native control using fixed test identities. It does not certify ordinary ANGE World/Host caller wiring, authentic product identity or Host-before-World closure. The state 2 record-return observation is logical; the test does not prove that GPU work was still executing at the instant of cancellation or that only part of a readback had completed. Result bytes are deliberately unavailable after cancel; the separate positive driver proves the exact scalar output.
