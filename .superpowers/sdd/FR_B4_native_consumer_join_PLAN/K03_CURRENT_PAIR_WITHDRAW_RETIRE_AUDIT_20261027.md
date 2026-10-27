# K03 current-pair withdraw/retire lifecycle audit — 2026-10-27

## Scope and result

This audit follows the exact current editor/DSO pair through the external
particle owner withdrawal, NativeUse retirement, Busy/Withdrawing guards, and
final authenticated Registry release order. It distinguishes logical
withdrawal and metadata release from physical/native disposal. The result is
locally fail-closed and ordered; no public `Released`, packet, completion,
readback, Ready, Verified, or GPU claim is made.

## Exact pair and source provenance

- Godot editor SHA-256: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- ANGE DSO SHA-256: `a087a9054c1079efdec85a91759647bea37aa46a0b1fd4d09a7b26ed4b9713e6`
- Godot source basis: `d/b4-native-output-20261027`, commit `1753faf66685154b1ffe2482bfe367c6039d4216`
- ANGE source HEAD used for DSO: `695921a207d89b27da6687453eba6a4ff2ec383d`
- Host: non-headless Metal/mobile, Apple M3 Pro.

Exact source and raw-log hashes are in the adjacent JSON.

## Owner and withdrawal order

The source order is explicit:

1. `particles_free()` marks `external_free_requested`, retains the original RID,
   calls `withdraw_native_owner()`, sends the dependency deletion notification,
   queues the particle on `external_retirement_list`, and attempts one bounded
   finalization.
2. `withdraw_generation()` accepts only the exact eight-byte generation and
   then tombstones admission. `withdraw_native_owner()` marks the generation
   withdrawn and closes admission on every occupied or nonempty NativeUse
   slot. Accepted closed lists remain owned by their original RD frame.
3. `_finalize_external_particle()` calls `retire_completed_native_uses()` and
   stops unless `can_destroy()` is true. `can_destroy()` requires withdrawn,
   no `native_effects`, no occupied slots, and every NativeUse empty.
4. Only after the previous checks does it verify that the original RID still
   names the same particle, call `release_native_owner()`, and then clear the
   particle's external pointer, destroy the storage object, and free the RID.
5. `ExternalOutputStorage::~ExternalOutputStorage()` asserts that the owner
   serial is already cleared and that authenticated Registry release was
   observed. Silent owner loss is therefore fail-closed.

`release_native_owner()` returns `ERR_BUSY` until `can_destroy()` succeeds. It
performs the authenticated Registry release, clears the owner serial only on
`OK`, and records an internal release observation. That observation is core
lifecycle evidence; it is not exposed as a public `Released` state.

## NativeUse retirement boundary

`RenderingDeviceNativeUse::is_complete()` requires closed admission, no unknown
state, at least one graph and native row, every graph sealed and returned, an
issued native row for every graph slot, matching completion, driver return,
disposal return, and delivered readback state. `retire_if_complete()` clears
its graph/native/readback rows only after that predicate succeeds.

`ExternalOutputStorage::retire_completed_native_uses()` visits every slot and
calls `retire_if_complete()`. A pending or unknown native use remains nonempty,
so `_finalize_external_particle()` returns false and the retirement list keeps
the owner node alive. No counter, withdrawal bit, or metadata release can
short-circuit the NativeUse predicate.

`particles_external_poll()` validates the exact generation and exposes only
`Created` or `Withdrawing`. It does not expose `Released`, completion, or
physical disposal. The public path therefore cannot turn a logical withdrawal
or Registry release observation into an external completion receipt.

## Current-pair focused evidence

`K03_CURRENT_PAIR_WITHDRAW_RETIRE_AUDIT_20261027_NATIVE.log`:

- command: `bin/godot.macos.editor.dev.arm64 --test --test-case='[B4]*native*' --no-colors`
- result: rc `0`, 2 test cases, 166 assertions, 0 failures.
- covers the NativeUse capture/issue/completion/readback/retire guards and
  unsupported native-output attachment surface.

`K03_CURRENT_PAIR_WITHDRAW_RETIRE_AUDIT_20261027_EXTERNAL.log`:

- command: `bin/godot.macos.editor.dev.arm64 --test --test-case='[B4]*external*' --no-colors`
- result: rc `0`, 3 test cases, 45 assertions, 0 failures.
- covers exact external configuration admission and the error-only external
  ports; no particles, generation, lease, or packet field is fabricated.

These focused tests exercise lifecycle refusal and internal state guards. They
use a bounded test driver where applicable; they do not prove a production
external producer, physical GPU disposal, or a public Released state.

## Claim matrix and remaining owner boundary

All positive integration claims remain false: U08, E09, operation, completion,
readback authority, packet, physical disposal, Released, Ready, Verified, and
GPU qualification.

The next positive input remains the A/U08 producer admission plus same-request
E09 source/operation association. Godot's local order is already constrained
around that input; no safe local change can invent it from a RID, serial,
withdrawal bit, NativeUse emptiness, or Registry release observation.
