# K03 native completion/readback join audit — 2026-10-27

## Scope and result

This audit follows the real production chain from queue issue facts through
NativeUse completion and BufferGetDataResult delivery. It identifies the first
missing owner boundary without treating metadata, a test driver, RID, ObjectID,
or a graph handle as operation/completion/readback authority.

## Implemented Godot chain

1. `RenderingDeviceDriver::QueueCallReport` carries typed `NativeCallFact`
   issue state, backend serial, queue index, completion capability, and exact
   completion serial. Vulkan and Metal expose bounded poll/wait/recycle/release
   observations for those actual calls.
2. `RenderingDevice::_native_output_issue_uses()` joins graph records to those
   exact facts. `_native_output_complete_uses()` records only a matching
   `FencePollResult`; native errors remain terminal/UnknownRetained.
3. `_stall_for_frame()` performs the original frame/fence boundary, driver
   return, disposal return, and frame owner return. `NativeUse::is_complete()`
   requires closed admission, returned graph/native rows, matching completion,
   and delivered readback state before retirement.
4. Real buffer readbacks add graph handles and exact source/destination/range
   provenance to the active NativeUse. The existing asynchronous
   `BufferGetDataResult` path copies staging bytes in `_stall_for_frame`, updates
   issued/completed/error/delivered state, and mirrors that state back to the
   NativeUse cell.
5. Forward+ and Mobile renderer paths detect external particle objects and set
   `instance_count = 0` before ordinary simulation getters or draw-pass use.
   They deliberately wait for an authenticated packet/read-use bridge.

This is a real completion/readback consumer chain inside Godot. It is not a
public external producer or packet authority.

## First missing owner boundary

The first missing positive input occurs before `ParticlesStorage` can issue an
external lease:

- `particles_external_create()` refuses without the original U08 producer
  admission;
- `particles_external_acquire()` refuses without the original source/operation
  owner;
- `particles_external_seal()` has no authenticated lease to record;
- no same-request source/operation key reaches the graph producer;
- no E09 closed-prefix receiver publishes a packet/readback request to the
  renderer.

Consequently, the existing Godot completion/readback chain cannot be promoted
to the external particle API. Adding a packet from NativeUse, a completion from
the queue serial, or readback authority from `BufferGetDataResult` would be a
G1 violation. The correct local behavior remains unavailable/fail-closed and
renderer `instance_count = 0`.

## Owner handoff minimum

The external owner must provide, before any positive join:

1. authenticated U08 producer admission and bounded object/lease ownership;
2. same-request source/operation association for each graph append;
3. E09 closed-prefix receiver and packet/readback range contract;
4. explicit completion and physical-disposal receipts that can be joined to
   the original NativeUse/frame return.

Only after those inputs exist can the existing Godot queue/readback chain be
connected. No local source fix can safely invent them.

## Evidence and limits

- Source branch: `d/b4-native-output-20261027`.
- Source commit: `1753faf66685154b1ffe2482bfe367c6039d4216`.
- The audit is source-based and reuses the previously recorded fixed-pair
  marker evidence; it does not rerun a binary from the mixed persistent host.
- No operation, completion, readback bytes authority, packet, physical
  disposal, Released, Ready/Verified, or GPU qualification is claimed.
