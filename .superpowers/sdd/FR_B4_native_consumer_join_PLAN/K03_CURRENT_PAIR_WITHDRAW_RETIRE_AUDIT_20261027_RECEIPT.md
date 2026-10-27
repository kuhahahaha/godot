# K03 current-pair withdraw/retire audit receipt — 2026-10-27

- Exact editor `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853` and ANGE DSO `a087a9054c1079efdec85a91759647bea37aa46a0b1fd4d09a7b26ed4b9713e6` were used.
- Withdrawal closes future admission; accepted frame-owned NativeUse lists remain until their own completion/return/disposal/readback predicate retires.
- Finalization requires withdrawn + no native effects + empty slots + empty NativeUse, then authenticated Registry release; destructor asserts that order.
- Focused current-pair tests: `[B4]*native*` 2/166 and `[B4]*external*` 3/45, all rc0.
- Logical withdrawal and internal Registry release observation are not public Released or physical disposal receipts.
- No U08/E09, operation, completion, readback authority, packet, physical disposal, Released, Ready, Verified, or GPU claim.
