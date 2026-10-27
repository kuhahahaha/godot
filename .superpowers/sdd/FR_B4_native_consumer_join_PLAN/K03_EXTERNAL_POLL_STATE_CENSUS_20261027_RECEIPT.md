# K03 external poll/withdraw state census receipt — 2026-10-27

- Valid generation states: `Created` and `Withdrawing` only.
- Exact eight-byte generation equality precedes state return.
- Withdrawal is idempotent and closes new admission for occupied/nonempty NativeUse slots.
- Finalizer eligibility, NativeUse retirement, Registry release, and public poll remain separate.
- No `Released` state is inferred after frame waits, counter changes, or metadata release; freed rows become invalid poll targets.
- Source-only census; no operation/completion/readback/Ready/Verified/packet/physical/GPU claim.
