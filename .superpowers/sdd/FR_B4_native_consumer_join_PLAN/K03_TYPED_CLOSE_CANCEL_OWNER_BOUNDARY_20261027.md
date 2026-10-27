# K03 typed-close cancel owner boundary — 2026-10-27

## Finding

The public cancel method is bound on `RenderingServer` and forwarded by
`RenderingServerDefault`, but the matched RD implementation has no
authenticated lease/ticket to cancel. On the render-thread RD path, the
current fail-closed behavior is therefore correct:
`particles_external_cancel` returns `ERR_INVALID_PARAMETER` and does not mutate
a particle row, close a ticket, or release metadata. The wrapper may refuse
wrong-thread calls earlier with `ERR_UNAVAILABLE`.

## Source-confirmed path

At Godot branch `d/b4-native-output-20261027`:

1. `RenderingServer::particles_external_cancel` is bound and forwarded by
   `RenderingServerDefault` to `ParticlesStorage`.
2. `ParticlesStorage::particles_external_acquire` rejects before issuing a
   lease because the original source/operation owner is unavailable.
3. `particles_external_seal` rejects an invalid lease and never records output.
4. `particles_external_cancel(RID, PackedByteArray)` returns
   `ERR_INVALID_PARAMETER` unconditionally.
5. `ExternalOutputStorage` exposes withdrawal and final
   `release_native_owner()`, but no cancel-ticket or operation-close method.
6. `Registry::native_output_release_object()` only releases an already
   authenticated metadata output row after the original owner has retired its
   obligations. It is not a cancellation primitive.

The existing contract test also treats empty cancel input as refusal. No source
caller provides an authenticated upload/record-prepared ticket to this path.

## Minimum owner input

The A/original producer owner must supply, before a positive cancel operation:

- the same authenticated ticket used by upload/record-prepared;
- the same-request operation/source association;
- a checked cancel disposition that closes that ticket's upload and
  `record_prepared` obligations;
- an explicit completion/retirement receipt before final Registry metadata
  release.

Only that owner can decide whether cancellation is accepted, already complete,
or unknown-side-effect. Godot must preserve `ERR_INVALID_PARAMETER` or
`ERR_UNAVAILABLE` until the ticket exists. It must not reinterpret a generation
byte string, RID, ObjectID, native serial, qualification ticket, or successful
Registry release as cancel authority.

## Queue decision

This is an external A/owner blocker for the positive cancel path, not a reason
to stop the queue. Local Godot lifecycle, NativeUse completion, and fail-closed
renderer work continue independently. `release_native_owner()` remains the
final metadata release after physical/native obligations; it is intentionally
not changed into cancel.

No operation, completion, readback, Ready/Verified, packet, physical disposal,
or GPU qualification claim is made.
