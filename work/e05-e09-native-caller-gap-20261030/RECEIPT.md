# E05 → E09 native caller gap — 2026-10-30

This package maps the real native chain from SceneReady E05 installation through
E09 submission, exact completion, readback, and release. The first refusal is
`read_storage_owner_inputs()` in ANGE, which intentionally returns
`host.storage_owner_transport/E05/UNAVAILABLE` because the authenticated upstream
owner transport is absent. No Godot-side failure propagation or retirement defect
was found after that boundary.

The source census names every real function and the owner/context inputs needed to
cross it. Godot already rejects unissued/failed submits, tracks exact submission
serials, waits/recycles matching completion, seals readback mappings, and withholds
callbacks on map/extent failure. ANGE E09 already validates owner RT, native shader /
pipeline RIDs, E05 backing serials, ticket revisions, upload segments, and same
request readback extent. A new caller or fallback would violate these contracts.

## Fresh focused verification

- RenderingDevice: rc 0, 12/12 cases, 214/214 assertions.
- Qualification: rc 0, 102/102 cases, 1896/1896 assertions.
- K03: rc 0, 44/44 cases, 1153/1153 assertions.
- R01: rc 0, 16/16 cases, 293/293 assertions.
- Current Metal pair with editor SHA in `EDITOR_SHA256.txt` and DSO SHA in
  `DSO_SHA256.txt`: VFX, no-VFX, invalid-VFX each rc 2 at the same E05 owner
  transport refusal. No `U01_ENTER_WORLD`, operation, completion, readback, Ready,
  or GPU result is claimed.

The required joint command is recorded in `COMMANDS.txt`: build/use the fixed
Godot editor + explicit ANGE DSO pair, then run the three probes. No fallback or
synthetic owner is introduced.
