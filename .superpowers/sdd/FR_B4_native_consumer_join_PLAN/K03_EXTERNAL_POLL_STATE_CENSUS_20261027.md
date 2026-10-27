# K03 external poll/withdraw state census — 2026-10-27

## Finding

The public generation state is deliberately limited to `Created` and
`Withdrawing`. Generation equality is checked before any state is returned;
withdrawal tombstones admission for occupied/nonempty NativeUse slots, while
finalizer eligibility and Registry release remain separate internal gates.
The path never fabricates `Released` from a poll, frame wait, counter change,
or metadata release.

## Source-confirmed behavior

- `particles_external_poll()` rejects an invalid particle/generation pair with
  `invalid_external_generation`.
- A valid non-withdrawn generation returns `{error: OK, state: Created}`.
- A valid withdrawn generation returns `{error: OK, state: Withdrawing}`.
- `withdraw_generation()` requires the exact eight-byte generation and is
  idempotent after the first accepted withdrawal.
- `withdraw_native_owner()` closes admission for slots carrying an occupied or
  nonempty NativeUse record; accepted work remains owned by its original frame.
- `can_destroy()` requires withdrawal, no native effects, and empty slots/uses.
  Only then may `release_native_owner()` call authenticated Registry release.
- After the row is actually freed, a later poll cannot expose Released; the
  particle/generation lookup is invalid instead.

The comment and implementation explicitly distinguish finalizer eligibility
from a public release observation. This preserves G1 while allowing the queue
to continue on source/owner work.

## Evidence boundary

The existing contract tests exercise public port registration and refusal for
empty/unsupported poll, cancel, and withdraw inputs; NativeUse tests exercise
completion/readback retirement separately. This census is source-only and does
not rerun a binary from the mixed host.

No operation, completion, readback bytes, packet, physical disposal,
Released/Ready/Verified, or GPU qualification claim is made.
