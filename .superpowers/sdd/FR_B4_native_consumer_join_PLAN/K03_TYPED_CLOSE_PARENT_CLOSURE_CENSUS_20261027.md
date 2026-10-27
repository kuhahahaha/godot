# K03 typed-close parent closure census — 2026-10-27

## Finding

The authenticated native-output parent has a real fail-closed close protocol.
It marks the parent closing, keeps the same service node while children or
reserved credits remain, and returns Busy. Detach and parent/holder/device
reference release occur only after all output rows, children, lease slots, and
storage bytes are gone.

## Source-confirmed order

1. `Registry::native_output_close()` validates the registry instance and
   attachment serial, then checks the original owner thread.
2. It sets `attachment->closing = true` and treats children, reserved
   lease/storage credits, and output serials as live obligations.
3. For live obligations it requests the same `NativeServiceNode`, optionally
   services one bounded turn on the render thread, and returns `ERR_BUSY`.
   The parent remains attached; no force-detach or synthetic completion exists.
4. `Registry::native_output_service()` validates the same main RD, device epoch,
   render-thread identity, and attachment serial. It services only already
   accepted native-use/storage retirement work and requeues a full bounded turn.
5. Only when no live obligation remains does close detach the service node,
   decrement holder/library/device references, delete the attachment, and allow
   reclaim/finalization.

`release_native_owner()` is downstream of the particle/storage `can_destroy()`
gate and is a final metadata release. It is not a close shortcut, cancellation
primitive, or GPU completion receipt.

## Evidence boundary

The existing source tests cover retained children blocking close and release of
exact credits before a second close succeeds. This census records the source
invariant and the test cases; it does not rerun a binary from the mixed host.
The fixed-pair marker evidence separately proves only VFX source-stage and
extension/world/unload status.

## Remaining owner seam

The parent closure protocol is ready to receive the original producer's typed
child/operation receipts. It cannot create them. A positive close still needs
the producer-owned ticket, same-request association, physical-child disposal
receipt, and completion boundary described in the cancellation and disposal
censuses. Until then, the correct result is Busy/Unavailable with the parent
and credits retained.

No operation, completion, readback, Released, Ready/Verified, packet, physical
disposal, or GPU qualification claim is made.
