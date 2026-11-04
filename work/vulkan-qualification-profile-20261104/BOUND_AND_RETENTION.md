# Vulkan qualification submission bound and original source retention

This note describes the production candidate in this isolated worktree. Runtime acceptance is recorded separately after executing its source-matched editor; the argument below is not a substitute for that run.

## Checked account

The original qualification account remains 4096 total facts. Let G be the conservative original graph count, C the already recorded main command count, and P the original maximum transfer-worker pool size. The existing checked source-slot calculation uses:

```text
F = G + 1
M = C + F
W = P * F
S = M + W
```

`qualification_source_slot_bound` checks capacity and multiplication using division before publishing a result. The inherited driver profile rejects zero/invalid frame counts and overflow of `M + W`. Vulkan now supplies `N = S` for `S <= 4096`, otherwise zero. The original qualification gate still requires `G + N <= 4096`; a nonzero backend bound alone does not grant admission. No account size or qualification identity changes.

Boundary examples used by the actual Registry capacity tests:

| G | C | P | F | M | W | N | G + N | Gate |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 314 | 0 | 11 | 315 | 315 | 3465 | 3780 | 4094 | accept capacity |
| 315 | 0 | 11 | 316 | 316 | 3476 | 3792 | 4107 | reject before graph/source publication |

The graph counts include original upload rows, bounded readback partitions and Capture/Body, with the existing extra frame/membership allowance. No worker or continuation frame is omitted. The native driver's oversized V2 prepare uses the same original allocations and valid descriptor shape; with four upload segments and 9824 expected result bytes, the original calculation reaches G315 on the eleven-worker host. Exact runtime traces must confirm that observed profile before claiming its capacity branch.

## One actual submission source per slot

The original graph builder can split main command buffers at compute/draw boundaries. `RenderingDevice::execute_chained_cmds` calls the observed queue API once per actual main slot and stops at the first failure. `_submit_transfer_worker` calls it once per worker submission. The original frame bound includes a fresh bounded worker pool for every possible continuation frame.

`RenderingDeviceDriverVulkan::command_queue_execute_and_present` has a single `vkQueueSubmit` callsite. All command buffers, wait semaphores and signal semaphores of that call are in the same `VkSubmitInfo`. An empty command-buffer list creates no submission source. A pre-submit reservation/flush/capacity failure submits nothing; an actual failed submit retains one original issued-unknown source without retrying. `qualification_prepare_segment`, `begin_segment` and `end_segment` make no hidden Vulkan submissions.

Thus M main slots plus W worker slots retain at most M+W original submission sources, with no Metal wait/signal multiplier or hidden-copy-frame term.

This bound is for ticket-retained submission sources, not arbitrary WSI report entries. The bounded original record profile refuses swapchains before any effects. Qualification mapping marks presentation-only facts unknown and does not insert them as native ticket facts. This package does not change WSI qualification policy.

## Original observation and physical lifetime

- Each original nonempty queue call reserves a monotonic NativeCall serial before submitting. A main intermediate call without a frame fence has its own VkFence; final main calls and workers use their original fence. Observations are not replaced by a later tail fence.
- Main frame retirement observes each actual source, joins and retains ticket sources, then recycles intermediate sources and the frame fence. Worker retirement holds the original pool/thread/operation locks while joining its exact submission report and retaining ticket sources; its observer-pending state prevents early fence recycling.
- Original fence return removes the old VkFence handle and stores the immutable completion snapshot. Monotonic call/submission serials reject exhaustion; old observations cannot query a subsequently reused fence. Cell collection requires zero observers and zero waiters. Wait/recycle paths re-find the original serial after releasing locks or waiting.
- The ticket's original reducer requires successful exact native observations, graph/frame return, zero frame references and no mapping failure before unpinning resources. Release checks RT/frame/physical obligations and original forward/reverse resource edges before acknowledging and dropping native observers.
- Actual resource retirement publishes its original free event through the existing driver callback. Native completion or logical ticket acknowledgement does not manufacture that physical event.

Independent source reviewers `/root/vulkan_submission_bound_proof` and `/root/astra_vulkan_retention_audit` traced these paths. The latter also reviewed the actual header/tests candidate and found no blocking omission under the existing success-only-return policy.

## Preserved failure policy

Issued-unknown/failed submits are not promoted to success when a fence later signals. They can retain the original frame, worker and NativeCall indefinitely; observer release cannot acknowledge an unresolved original source. Existing fault tests settle an injected successful submission only in fixture teardown; that is not a production recovery mechanism.

The original `vkQueueSubmit(VK_ERROR_DEVICE_LOST)` branch records the native outcome and retains the existing fatal device-loss policy. This candidate does not add recoverable submit-loss behavior or claim automatic physical exit after an unknown native result. Existing direct native tests must continue to prove no false completion/ACK on those paths.
