# Independent review — K03 external poll/withdraw state census — 2026-10-27

- Reviewer: `poll_state_census_review2` (non-author)
- Source basis: `d/b4-native-output-20261027`, commit `1753faf666`
- Result: **limited-scope accept**, no actionable P0/P1/P2/P3.
- Tests: not rerun; source-only review.

The reviewer confirmed that `particles_external_poll()` requires exact
eight-byte generation equality and exposes only `Created` or `Withdrawing`.
It does not infer `Released`, completion, readback, or returned charges.
`withdraw_generation()` rejects a wrong generation, marks withdrawal, and
`withdraw_native_owner()` tombstones admission for occupied/use-bearing slots
while preserving accepted closed lists.

Registry release and row retirement remain internal and are explicitly separate
from public Released state. No authority or positive qualification is
fabricated.
