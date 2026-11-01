# Native V2 same-ticket readback checkpoint

2026-11-01 (Asia/Shanghai). Native implementation is in `/Users/macbookair/Documents/Work/godot_ANGE`, base `98d315818db624e4b81f867f718ec5fac9beb385` plus named source in `build-source-before.json`; the exact build source window had no drift. Native source version is not inferred solely from the executable version string.

## Implemented and verified

Additive V2 Record448/Report328, Range48/Observation56, explicit one/two uniform-set counts and original set1 binding, three same-ticket BufferGetDataResult instances, exact source range/aggregate offsets, unchanged V1 byte layouts and original QRT1–9. QRT10 records the original prepared V2 ticket; QRT11 prepares it; upload remains QRT8. Existing observe/facts/copy/cancel/release ownership remains intact. All Method ranges must complete; cancellation and failures retain original native/graph/physical obligations. Same-frame Body→copy dependency and original returned Body native proof for later-frame copies are checked.

- `scons platform=macos target=editor tests=yes -j4`: rc0 (425.45 seconds); `build.log` and `build-result.json`.
- Source drift: empty. Fixed host in `PINNED_HOST.json`: monolithic executable SHA256 `9e24b2281bf0b02735fb21b3e433a6142fe945d2856fd41c6f5f7d9965ab4575`; controlled-build JSON SHA256 `8291385355cd7313ff542b7bc3caf3016624f56fc52738ea969171593599945e`.
- Focused multi-range/two-set/typed preflight: 7 cases, 456 assertions, rc0.
- Qualification neighbors: 184 cases, 3134 assertions, rc0. Explicit controlled diagnostic output is retained; this is not a generic log-clean claim.
- Method/reload neighbors: 10 cases, 515 assertions, rc0; three actual RELOAD_CAPTURE callback routes executed.
- Particle selection: 28 registered cases, 149 assertions, rc0. One trail GPU case explicitly returned UNRUN and is not GPU evidence; process GPU case was excluded. Source-extracted particle fixtures have separate results.
- Real ordinary Metal Mobile/safe-thread controls: V2 positive (3 distinct original async serials, 44 exact bytes, unchanged two tail guards, 11 successful facts and physical acknowledgements); V2 uploaded-prefix cancellation (6 settled facts, zero result); recompiled V1 ticket (5 facts and exact 16 bytes); V1 upload (6 facts). All runner checks and process exits passed. Receipt: `runtime/results/20261101-105953/results.json`.

## Store allocator correction — verified RED/GREEN

Root review correctly distinguished the logical E24 Store destination from its actual E05 allocator. The prior pinned host admitted/replayed the original E05 prepare and all four bounded uploads, then refused the original METHOD_DESTINATION/METHOD_RESOURCE range at kind10 with ERR_UNAUTHORIZED=4/reason16. The owner-mismatch negative (E24 issuer tag plus nonzero E05 backing) was separately refused before ticket publication. Exact RED: `runtime/results/20261101-110408/STORE_DESTINATION_RED.json`.

The corrected predicate retains logical role METHOD_RESOURCE18 and authenticates actual E05 owner_kind, original nonzero backing ref/instance/child serial, physical cell, generation, RID/backing and live state. Bare RIDs are refused. The corrected host (Store + independently reviewed 3D R1) built rc0 with no source drift; pinned identity is `../physics-wrapper-finish-recovery-20261101/PINNED_HOST.json`, executable SHA256 `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58`. Store Metal GREEN returned all 3 original ranges, exact 44 bytes, tail guards, known native facts, physical acknowledgement and clean exit. Exact receipt: `runtime/results/20261101-111020/results.json`. Fresh qualification neighbors again passed 184 cases/3134 assertions. This fixes native Store-role/E05 admission, not an ANGE provider or Store-method acceptance.

## Boundaries

The native control is not a real ANGE producer/request, all 23 Method occurrences, E23/E24 provider grant, separate owner-thread upload caller, typed-close, G1, or full texture/sampler production qualification. The typed preflight authenticates original children; a normal RID alone is not authority and the production typed owner/create pathway remains subject to the original owner integration. Further root/A integration consumes the frozen SDK snapshots in `work/native-callable-handoff-20261101/v2/`.
