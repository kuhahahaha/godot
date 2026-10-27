# Native bridge handoff current recheck

Date: 2026-10-28

## Result

The reviewed native bridge handoff commit `cfada39de8cbd730df2d08b7765bc26c2abfbc81` resolves in the Godot repository and contains 42 selected native-owned paths. The retained historical receipts report a successful committed host: 161 CPU cases / 3001 assertions, six QRT Metal controls, and nine PHS1 rows / 233 checks. Those runs are historical evidence from the handoff package, not a fresh runtime execution in this recheck.

Comparing the committed handoff postimages with the current mixed checkout gives:

- 23 of 42 paths byte-identical
- 19 of 42 paths different
- 0 missing paths

The historical clean `/tmp/k03-native-closure-20261026` worktree is no longer present, so no current clean-worktree or fresh runtime claim is made. The 19-path difference is the exact integration boundary; no blind cherry-pick or mixed-tree overwrite was performed.

The remaining A/owner boundaries in the handoff remain separate: canonical ANGE fixed-pair acceptance, same-request R01 operation/completion/readback, and SeparateRT deployment mailbox allocation. No fallback, synthetic authority, or claim promotion was introduced.
