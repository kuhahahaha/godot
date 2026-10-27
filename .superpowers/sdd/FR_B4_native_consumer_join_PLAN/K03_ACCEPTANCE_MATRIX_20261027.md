# K03 acceptance matrix — 2026-10-27

This matrix consolidates the independent K03 source and current-pair packages
and separates landed local evidence from the external owner gates. It does not
declare the long goal complete.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| VFX projection consumer census | `1921759a6e` | `ff_vfx → WorldLoader → compile_system_bundle → EcsWorld validation`; `BridgeParticle` consumes ECS `Particle` only | projection → producer/operation owner |
| typed-close cancel owner boundary | `2008230c5f` | cancel binding/forwarder and fail-closed refusal | authenticated upload/record ticket and cancel disposition |
| typed-close parent closure | `82604358c4` | live obligations keep parent/service node attached; bounded Busy/requeue; detach only empty | typed producer child/operation receipts |
| external poll state | `e8f4781870` | exact generation; public `Created`/`Withdrawing`; withdrawal admission tombstone | owner completion/release observation |
| completion/readback join audit | `8b3b0af0ae` | QueueCallReport/NativeUse/fence return; BufferGetDataResult state delivery; renderer fail-closed guard | U08 admission + E09 same-request association before packet/readback publication |
| editor/DSO pair alignment | `409527e584` | exact current editor/DSO pair; non-headless Metal/mobile VFX/no-VFX source-stage, world entry, and explicit unload all rc0 | no external producer/operation/packet/readback owner |
| renderer/packet guard | `fc03fd5c18` | Forward+/Mobile external particle guard; NativeUse guard tests B4 20/419 and ParticlesTrail 1/5 rc0 | authenticated U08 producer and same-request E09 association |
| withdraw/retire lifecycle audit | `1d683b0c61` | withdrawal tombstone, Busy/Withdrawing, NativeUse retirement, authenticated Registry release and destructor order; focused B4 2/166 + 3/45 rc0 | public physical-disposal/Released receipt and owner producer association |
| final pair VFX/U01/close recheck | `6ead13473f` | final1 editor/DSO pair; VFX/no-VFX source-stage and world entry/unload rc0; explicit `exit_world()` active true→false and unload rc0 | superseded by final2 pair for current provenance |
| final2 pair VFX/U01/close recheck | `1b08fb7519` | final2 editor/DSO pair; VFX/no-VFX source-stage and world entry/unload rc0; explicit `exit_world()` active true→false and unload rc0 | U08/E09 operation/packet/completion/readback/physical/Ready/GPU evidence |
| D VFX projection validation/rollback audit | `290383df79` | source-only fail-closed projection publication, pre-mount validation/rollback, and same-path runtime erase guard | no binary attribution; no packet/operation/GPU authority |
| E03/E04 current-source owner boundary | `65b8469e1b` | source/CPU consumer evidence E03 17/1187 and E04 3/65; exact production UNAVAILABLE owner fields | accepted original preparation/source/session/detach authority and R01/K01 issuer |
| E34/P01 current owner boundary | `5bd8124839` | adapter no-root/drift fail-closed, selector 2, parent contract 4; clean bootstrap unavailable | authenticated parent root, OwnerPayloadBridge/E35 ABI, accepted bootstrap executor |
| I02 imported-texture consumer | `e645c1ca92` | CPU material/lint consumer exact 1/30 and family 19/7671 | no P01/E34/native/GPU/Ready authority |
| current-A consumer boundary set | `e1ed218af9` | E05/E17/E21/E22/E23/E30 source/CPU consumer slices with per-snapshot rc0 evidence | E20/owner/native/GPU/Ready authority remains absent |

## Fixed-pair freshness

The current non-headless Metal/mobile Apple M3 Pro pair is now aligned and
freshly probed:

- Godot editor SHA: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- ANGE source HEAD at final2 recheck: `53ff6aa1d48aa3697beff91eb31fee850ca326af`
- Final2 ANGE DSO SHA: `a078f6dec6661d5fb5217c8b95c59d162f18ac5aa8bc81cef9e9c85c548fcb45`
- VFX: `entries=2 systems=1`, extension/world/unload true
- no-VFX: `entries=1 systems=0`, extension/world/unload true
- explicit close: `exit_world()` active true→false, extension unload true

The preceding aligned pair `a087a905…` and final1 pair `149342a6…` remain preserved in their own receipts; final2 above is the current-pair provenance.

The earlier mismatched pair remains preserved as a negative ABI receipt:
`K03_CURRENT_PAIR_ABI_DRIFT_NEGATIVE_20261027.*` records the missing
`device_exit_register_scene_ready_v2` before world entry. It is no longer the
current pair after `K03_EDITOR_DSO_PAIR_ALIGNMENT_20261027.*`.

## Global claim matrix

U08 producer admission, E09 source/operation association, operation,
completion, readback authority/bytes, packet publication, physical disposal,
Released, Ready, Verified, and GPU qualification remain unclaimed. The queue
continues independently while those owner inputs are missing.
