# Author source review: applied generation identity coverage

2026-11-07. This is the candidate author's self-review, not independent approval. Root applied/formatted the three-file candidate after reported `b04a74f5f8`; reviewer performed no active-source edit, build, test or Git operation. Root build/runtime verification remains pending at this review.

## Verdict and scope

No source blocker found for this test-only extension. Changes are confined to the generation-selection test bridge/read-only helper and existing generation-selection lifecycle fixture. Actual alias READBACK integration, bounded driver simulation, runtime action admission and original cleanup code are unchanged.

The existing helper now checks all five exact 32-byte identities in fixed order: producer_tag, device_tag, layout_digest, mapping_digest, numeric_profile_digest. Each expected PackedByteArray must have size32 before memcmp. Each tag's byte17 is independently XOR-corrupted, expected to mismatch, then restored and expected to match. Thus removing any of the five helper comparisons produces a direct failed negative assertion. This is exact byte identity coverage at the selected mutation position, not five complete all-byte corruption sweeps.

Independent expected tags are freshly allocated and filled by memcpy; expected RID vectors are populated into initially empty independent vectors. They do not shallow-copy the parser config or keep its allocations alive. Parser tags are later overwritten with 0xa5, then tag and RID vectors are released. The surviving original owner must still match the independent expected values before OBSERVE retirement; actual original mesh/material hold counts must remain unchanged by parser destruction.

The added material-generation getter only reads the actual stable generation material row after owner/index checks. Expected generations come from real MaterialStorage before owner construction. A real priority mutation on material1 before snapshot makes its actual generation unequal to material0; nonzero and inequality assertions prevent swapped rows, missing snapshots, or a uniform constant from passing accidentally. The owner row values are checked immediately after creation and again after parser destruction. No physical/native geometry or readiness is synthesized.

## Original failure1 remains a stale-second-material case

MaterialStorage::material_set_render_priority increments generation (`material_storage.cpp:2939–2949`), subject to the existing immutable-busy guard. The new initial mutation occurs before the owner acquires any lease and before expected generation capture/owner snapshot. The existing `create_selection_component` failure1 mutation remains later: after row snapshot/install, before `acquire_generation_selection`. It changes material1 priority again and advances only its live generation.

The owner snapshot must therefore still match the original captured material1 generation while its acquisition refuses the now-stale second material with ERR_INVALID_DATA. Mesh and material0 leases remain retained; material1 has no acquired lease. The existing failure-state/withdraw/publication checks and exact bounded reverse retirement are unchanged. New expected-row assertions do not compare this stale row to the latest live generation or update it to hide the original negative. Failure2 still checks stored identities with zero acquired leases; failure3 has no installed owner and skips owner identity checks.

The fixture's existing attempted mutation/free of held material0 remains blocked by its real selection lease and does not alter the saved expected generation. The new initial material1 mutation cannot bypass that immutable guard.

## Alias integration neighbor identity proof

On isolated copies under `author-reverse-probe/`, reversed only the saved three-file candidate patch. No patch was applied to the active tree. Reversal succeeded without rejected hunks. The resulting full `test_production_request.cpp` SHA256 is exactly `117626be8e21cfdaf60c8b3fd8c0581d0ac5fa22b720a30c7a8c57674253b93d`, the frozen actual alias integration source independently reviewed and run by root previously. This full-file equality is stronger than searching for unchanged function names: the alias driver, ten append negatives, public-copy sentinels and successful drain are byte-identical outside the generation patch.

The reversed bridge/header and owner-state files also match the saved base hashes exactly. No production source is in this patch. No additional helper callsite was introduced into the alias fixture; it continues to use the same original generation creation helper without the new identity assertions.

## Current source identities

| Applied file | SHA256 |
|---|---|
| `tests/servers/rendering/test_external_output_custody_bridge.h` | `414dfa33493140bdd71685a53568beda7120a233dda23206db9941c8aba3ce57` |
| `tests/servers/rendering/test_external_output_owner_state.cpp` | `6a7a53e81b60ee3eb086d4501bc566c8a7685300dac01460c4199ccb3454e06f` |
| `tests/core/extension/test_production_request.cpp` | `c866c6172c7db9bbd230b6cea885ff2f82052cf98703ce9b92c49b0930a65425` |

The applied full test file differs from the package's older candidate full-file snapshot because the parent integrated the frozen alias tests in between; the isolated reversal proves the exact intended composition.

## Verification limits

Root's current build/runs are pending; this author review claims no passing new assertions or counterfactual execution. Root should run existing GenerationSelection fixtures and combined neighbors on its sole lane. No two-actual-mesh unequal-flattened native planner/acquire coverage, actual surface preparation, public Create readiness, seal, draw, GPU ordering or qualification is added by this change.
