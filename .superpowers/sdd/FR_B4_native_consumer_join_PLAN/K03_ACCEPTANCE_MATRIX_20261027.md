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

## Fixed-pair freshness

The current non-headless Metal/mobile Apple M3 Pro pair is now aligned and
freshly probed:

- Godot editor SHA: `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- ANGE source HEAD: `695921a207d89b27da6687453eba6a4ff2ec383d`
- ANGE DSO SHA: `a087a9054c1079efdec85a91759647bea37aa46a0b1fd4d09a7b26ed4b9713e6`
- VFX: `entries=2 systems=1`, extension/world/unload true
- no-VFX: `entries=1 systems=0`, extension/world/unload true

The earlier mismatched pair remains preserved as a negative ABI receipt:
`K03_CURRENT_PAIR_ABI_DRIFT_NEGATIVE_20261027.*` records the missing
`device_exit_register_scene_ready_v2` before world entry. It is no longer the
current pair after `K03_EDITOR_DSO_PAIR_ALIGNMENT_20261027.*`.

## Global claim matrix

U08 producer admission, E09 source/operation association, operation,
completion, readback authority/bytes, packet publication, physical disposal,
Released, Ready, Verified, and GPU qualification remain unclaimed. The queue
continues independently while those owner inputs are missing.
