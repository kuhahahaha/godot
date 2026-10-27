# K03 acceptance matrix v37 — 2026-10-28

This successor matrix preserves v36 and adds current-tree SceneInstall/SceneReady V2 controls. The evidence is bounded to the existing dev binary and does not promote owner, operation, completion, readback, Ready/Verified, or GPU qualification.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| Godot current SceneReady V2 validation | `580f7148b1` | `*[SceneReady]*` 2/52 rc0 and `*scene-ready*` 11/145 rc0 | no fresh build, canonical ANGE fixed-pair, production owner/operation/completion/readback, Ready/Verified/GPU claim |

The v36 fixed editor/DSO pair and all authority claims remain unchanged.
