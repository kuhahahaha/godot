# K03 acceptance matrix v45 — 2026-10-28

This successor matrix preserves v44 and adds the D-side capture reader null-output guard repair. Guards execute after schema/capacity preflight and before source getter/destination writes; zero-output captures remain valid.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| ANGE capture reader null-output guards | `b887d2902` | Curve 19/2277 rc0, image 18/528 rc0, six production reader TUs syntax rc0; empty capture success preserved | no owner permit/revision, publication, native/GPU, completion/readback, Ready/Verified claim |

The v44 fixed editor/DSO pair and all authority claims remain unchanged.
