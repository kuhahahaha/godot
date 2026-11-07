# Independent Clustered native pass source review

Status: **SOURCE_REVIEW_PASS_RUNTIME_PENDING**. No blocking source defect found.
This review ran no build, doctest, GPU process, mutation runtime, or commit.
The parent owns those gates. This is not a completion or visual-equivalence claim.

Reviewer: `camera_real_consumer_fixture_sol`, separately dispatched to inspect the
Clustered author's source. Scope is the three files in
`work/sprite-clustered-native-pass-selection-20261107/MANIFEST.json`, compared
directly with the provided `PREIMAGE.h/.cpp`. The frozen camera fixture and all
reviewed files were left untouched. `REVIEW.json` records exact source/preimage
identities; every current source SHA256 matches the author's manifest.

## Native branch matrix

| Native pass | Original and selected mono version | Multiview rule |
| --- | --- | --- |
| COLOR | COLOR_PASS | Flag-driven; view_count does not synthesize MV |
| SHADOW | DEPTH_PASS | count > 1 selects DEPTH_PASS_MULTIVIEW |
| SHADOW_DP | DEPTH_PASS_DP | count > 1 refuses |
| DEPTH | DEPTH_PASS | count > 1 selects DEPTH_PASS_MULTIVIEW |
| DEPTH_NORMAL_ROUGHNESS | DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS | count > 1 selects corresponding MULTIVIEW |
| DEPTH_NORMAL_ROUGHNESS_VOXEL_GI | DEPTH_PASS_WITH_NORMAL_AND_ROUGHNESS_AND_VOXEL_GI | count > 1 selects corresponding MULTIVIEW |
| DEPTH_MATERIAL | DEPTH_PASS_WITH_MATERIAL | count > 1 refuses |
| SDF | DEPTH_PASS_WITH_SDF | count > 1 refuses; world-space intent preserved |

The original template switch at PREIMAGE.cpp:438-491 was compared with the new
selector at render_forward_clustered.cpp:307-367. Noncolor flags remain ignored
for pipeline flags; counts <= 1 retain the original mono behavior. Refusal now
occurs before opening a list, including an empty list, instead of waiting for an
eligible element to reach the old in-loop unsupported-multiview check. This is
the requested boundary change; supported-path shader version choices match.

Original color dispatch explicitly allowed twelve cases. Independently parsing
that original list and enumerating 0..65535 against the new admissibility rule
found zero mismatches. Allowed numeric renderer flags are
`0,1,2,4,5,6,8,9,10,12,13,14`. All unknown bits are rejected by the mask; T+S
combinations 3/7/11/15 remain refused. The actual shader enum was read:
renderer T=1/S=2/MV=4/Motion=8 maps to shader T=1/S=2/MV=8/Motion=16;
LIGHTMAP=4 remains absent from pass-level base flags. The new color template
removes only the old flag-specific derivation and inclusion tests; no other use
of the removed `p_color_pass_flags` template argument was present.

## Surface behavior and actual consumer

At cpp:409 the new includes_surface is the exact negation of the original skip
predicate: noncolor always includes, mask zero includes, otherwise any original
renderer color flag intersecting the mask includes. It does not substitute
shader pipeline bits, which have different MV/motion positions.

At cpp:483 each surface reconstructs `pipeline_specialization` from the existing
base. At cpp:497-505 every surface resets pipeline flags from the frozen base,
then only a COLOR surface with uses_lightmap adds LIGHTMAP; otherwise only that
surface reads uses_forward_gi. These branches do not get frozen or carried over
from a previous surface. Soft shadow/projector/multimesh specialization, mesh
surface selection, material/debug branches, culling, primitive type, indexed144,
vertex input, motion-vector arrays, pipeline fallback, and draw calls remain in
the same per-element path. The actual PipelineKey consumes the selected
version/base flags/framebuffer_format at cpp:497-507. Wireframe is still read
from live render_list_params at cpp:508.

The dispatcher at cpp:663 consumes `native_pass.pass_mode` and retains all eight
native pass templates. The only remaining original p_params->pass_mode reads
inside the draw template are preexisting capture-test instrumentation at
cpp:648-649; neither shader selection nor list dispatch falls back to mutable
original pass inputs.

## Before-list provenance and direct material paths

The actual renderer has three list-opening entry families that invoke
_render_list. All were checked, not just the ordinary scene wrapper:

- cpp:697-703: query the supplied framebuffer's actual format, select, then
  draw_list_begin, dispatcher, and draw_list_end. Existing depth, opaque,
  motion, alpha, shadow, particle-heightfield and SDF call sites route here.
- cpp:3042-3046 `_render_material`: query its actual framebuffer format and
  select DEPTH_MATERIAL before its begin, then consume the selected value.
- cpp:3099-3126 `_render_uv2`: query/select once before its begin. The nine
  existing uv_offset assignments stay inside the draw loop. Each invocation of
  the material template packs the live offset at cpp:396-398. The final zero
  offset and true-to-false force_wireframe transition remain outside selection
  and still feed the final triangles invocation.

All `_render_list` call sites were enumerated. No remaining active-list
framebuffer_get_format query or old separate format argument feeds this path.
Other draw_list_begin sites use independent clear/effect drawing paths and do
not call this native list dispatcher. The removed RenderListParameters format
field had no other consumer in PREIMAGE.

## Tests and evidence limits

The four new doctests call the actual production selector/includes_surface via
a private friend. Expectations are not computed by calling a second copy of the
selector: the pass table names explicit historical shader enum targets, the
color table contains independent literal bit mappings, and inclusion rows give
explicit booleans for zero/single/mixed/unknown masks. Negative cases cover all
four T+S combinations, unknown low/high bits, three unsupported multiview modes
and invalid pass mode, checking every output field remains unchanged. Original
shader enums were independently checked against the numeric table. Test force
link and recursive discovery are present.

These tests exercise selection/helper branches. They do **not** execute actual
list creation or the per-surface PipelineKey/draw consumer, material/UV2 output,
lightmap/GI scene rendering, or GPU synchronization. For example, replacing a
draw-time use of native_pass with another input could leave selector tests
passing. That limitation is explicit in the author's README and is not being
promoted to runtime evidence here. The direct source audit above checks current
wiring; parent compilation, exact four-case discovery/execution, and relevant
ordinary renderer/real-GPU regression remain required. No test outcome or
mutation sensitivity was inferred from this read-only review.
