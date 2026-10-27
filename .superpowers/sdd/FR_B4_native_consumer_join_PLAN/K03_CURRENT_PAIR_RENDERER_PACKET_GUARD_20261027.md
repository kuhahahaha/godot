# K03 current-pair renderer/packet guard audit — 2026-10-27

## Scope and result

This package checks the exact current editor/DSO pair at the Godot renderer
boundary where an external particle object would otherwise enter ordinary
particle draw-pass code. It also reruns the focused NativeUse/packet guard
coverage in that pair. The result is a fail-closed negative boundary: the
renderer has an explicit guard, the internal NativeUse chain has real issue,
completion, readback, and retirement checks, and no authenticated external
producer or packet consumer is present.

The package therefore records useful native progress without converting a
local guard or test-driver fact into an external packet, GPU output, Ready, or
Verified claim.

## Exact pair and source provenance

- Godot editor SHA-256: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- ANGE DSO SHA-256: `a087a9054c1079efdec85a91759647bea37aa46a0b1fd4d09a7b26ed4b9713e6`
- Godot source basis: `d/b4-native-output-20261027`, commit `1753faf66685154b1ffe2482bfe367c6039d4216`
- ANGE source HEAD used for the DSO: `695921a207d89b27da6687453eba6a4ff2ec383d`
- Host: non-headless Metal/mobile, Apple M3 Pro.

The source SHA-256 manifest and raw log hashes are in the adjacent JSON.

## Renderer guard boundary

Forward+ `render_forward_clustered.cpp` and Mobile
`render_forward_mobile.cpp` both handle `RSE::INSTANCE_PARTICLES` as follows:

1. They call `particles_storage->particles_is_external()` before ordinary
   draw-pass and amount lookup.
2. For an external object they set `ginstance->instance_count = 0` and leave
   the ordinary draw-pass path.
3. Later particle setup accessors are themselves external-aware: amount and
   frame-counter reads return zero, the transform uniform RID is invalid, and
   `update_particles()` removes external rows without native simulation.
4. The source comment explicitly states that external output has no ordinary
   particle draw-pass or amount source until an authenticated packet/read-use
   bridge exists.

This is the correct G1 behavior. It prevents an external object from being
silently rendered through native particle state, while also refusing to claim
that `instance_count = 0` is a packet or GPU result.

## NativeUse and packet-consumer boundary

The internal production chain remains concrete:

- `_native_output_begin_capture()` admits only the main RenderingDevice on the
  render thread with a driver that supports native-call observation.
- Graph records are sealed only when the original graph resolves their command
  buffer slot. `_native_output_issue_uses()` accepts only typed queue reports
  with a known issue, queue index, completion capability, and completion serial.
- `_native_output_complete_uses()` records only a matching driver poll result;
  an unknown issue or native error keeps the use retained.
- Readback state stores the graph handle and exact source/destination/range,
  then requires an observed `BufferGetDataResult` owner, matching sizes,
  `OK`, and delivered state.
- `RenderingDeviceNativeUse::is_complete()` requires closed admission, no
  unknown state, returned graph/native rows, matching completion, and delivered
  readback before `retire_if_complete()` can clear the use.

These are internal provenance and lifecycle facts. They do not supply the
missing external source/operation association, an authenticated packet, or a
public readback authority.

## External storage and negative owner evidence

The current Godot storage path remains deliberately unavailable at the first
missing owner boundary:

- `particles_external_create()` refuses without original U08 producer
  admission.
- `particles_external_acquire()` refuses without the original source/operation
  owner and therefore cannot issue a writable lease.
- `particles_external_seal()` has no authenticated lease to record and returns
  an invalid-lease result.
- `particles_external_cancel()` remains error-only; it is not a same-ticket
  cancellation receipt.
- `particles_external_poll()` accepts only the exact generation and exposes
  `Created` or `Withdrawing`; it does not expose `Released`.
- `ExternalOutputStorage` retains per-slot NativeUse cells, but metadata
  reservation is not native allocation, source lease, graph append, packet
  publication, or physical disposal. Registry release remains the final
  authenticated owner release after all uses retire.

No producer owner, checked append tri-state, same-request operation key, packet
range, or exact-request readback is present for the renderer to consume. A
fallback registry, synthetic token, queue serial, RID, ObjectID, or test-driver
handle would violate the evidence boundary.

## Current-pair focused evidence

`K03_CURRENT_PAIR_RENDERER_PACKET_GUARD_20261027_B4.log`:

- command: `bin/godot.macos.editor.dev.arm64 --test --test-case='[B4]*' --no-colors`
- result: rc `0`, 20 test cases, 419 assertions, 0 failures.
- the log contains expected foreign-camera-source diagnostics; the suite
  remains green.

`K03_CURRENT_PAIR_RENDERER_PACKET_GUARD_20261027_PARTICLES_TRAIL.log`:

- command: `bin/godot.macos.editor.dev.arm64 --test --test-case='[ParticlesTrail]*' --no-colors`
- result: rc `0`, 1 test case, 5 assertions, 0 failures.

The B4 source tests exercise the real NativeUse state machine with a bounded
transfer driver and assert that unsupported external ports return
`ERR_UNAVAILABLE` without particles, generation, lease, or packet fields. The
driver is a test witness; it is not an external producer and does not qualify
GPU visibility.

## Claim matrix and next owner input

All positive integration claims remain false: U08, E09, operation, completion,
readback authority, packet, physical disposal, Released, Ready, Verified, and
GPU qualification.

The first missing positive input is the A/U08 producer admission plus the
same-request E09 source/operation association that must precede external lease,
graph append, packet publication, and exact readback. This is an owner handoff
boundary, not a reason to block independent native guard, lifecycle, and
source-audit work.
