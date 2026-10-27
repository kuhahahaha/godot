# K03 acceptance matrix — 2026-10-27

This matrix consolidates the independent K03 source packages and separates
landed local evidence from the external owner gates. It does not declare the
long goal complete.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| VFX projection consumer census | `1921759a6e` | `ff_vfx → WorldLoader → compile_system_bundle → EcsWorld validation`; `BridgeParticle` consumes ECS `Particle` only | projection → producer/operation owner |
| typed-close cancel owner boundary | `2008230c5f` | cancel binding/forwarder and fail-closed refusal | authenticated upload/record ticket and cancel disposition |
| typed-close parent closure | `82604358c4` | live obligations keep parent/service node attached; bounded Busy/requeue; detach only empty | typed producer child/operation receipts |
| external poll state | `e8f4781870` | exact generation; public `Created`/`Withdrawing`; withdrawal admission tombstone | owner completion/release observation |
| completion/readback join audit | `8b3b0af0ae` | QueueCallReport/NativeUse/fence return; BufferGetDataResult state delivery; renderer fail-closed guard | U08 admission + E09 same-request association before packet/readback publication |

## Fixed-pair freshness

The recorded non-headless Metal/mobile Apple M3 Pro marker pair remains valid
for its original inputs:

- Godot editor SHA: `0c4e23c1ae93ba0455bdda99674f221677eb3c4ee051d2e55b3337520025bdba`
- ANGE source HEAD: `b2038e9a5be40624f6b19613f209422da49b4355`
- ANGE DSO SHA: `dfdb1e94e2694cfde7b0259efc232c42fb3388a73c63847504eb7c3606c6d566`
- VFX marker: `entries=2 systems=1`, extension/world/unload true
- no-VFX marker: `entries=2 systems=0`, extension/world/unload true

The current ANGE checkout has since moved to source HEAD
`069196f3a9e8c1d0f36e1f61daf5a3bb2daaa019` and DSO SHA
`c82f09f62b84af0ac14965a7bab29ab62e408aa5106d3264a2be7eb91777cff0`.
The fresh pair was probed non-headless on Metal/mobile. Both VFX and no-VFX
stop before world entry with `U01_EXTENSION_STATUS=1`, rc=2, and the first
causal error `device_exit_register_scene_ready_v2` missing from the editor.
This is recorded in `K03_CURRENT_PAIR_ABI_DRIFT_NEGATIVE_20261027.*` and is
classified as an editor/DSO ABI mismatch, not VFX or close behavior.

## Global claim matrix

U08 producer admission, E09 source/operation association, operation,
completion, readback authority/bytes, packet publication, physical disposal,
Released, Ready, Verified, and GPU qualification remain unclaimed. The queue
continues independently while owner inputs are missing.
