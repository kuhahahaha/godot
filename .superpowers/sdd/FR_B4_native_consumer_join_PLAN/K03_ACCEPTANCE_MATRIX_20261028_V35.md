# K03 acceptance matrix v35 — 2026-10-28

This successor matrix preserves v34 and records a current-tree recheck of the reviewed 42-path native bridge handoff. The handoff commit and historical controls are real, while the current mixed-tree integration boundary is measured explicitly. No historical result is re-run or promoted to current fixed-pair/production authority.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| Godot native bridge handoff current recheck | `cfada39de8` | Handoff commit resolves; 42 paths compared against current tree: 23 exact, 19 different, 0 missing; retained historical host evidence is 161/3001 CPU, QRT6, PHS1 9/233 | Current mainline integration remains open; no fresh runtime, canonical ANGE pair, same-request R01 completion/readback, SeparateRT mailbox, or Ready/Verified/GPU claim |

The v34 fixed editor/DSO pair and all authority claims remain unchanged.
