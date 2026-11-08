# Scene117 local height10 / neighbors12 / capture16 receipt

2026-11-07. All nine original commands in `SCENE117_HEIGHT_NEIGHBORS_RESULT.json` returned0. Root executed the unified build and serial local runtime; this receipt independently checks saved source pins, raw logs, every numeric sample, renderer banners and CPU XML. Exact commands, samples, file identities and limits are preserved in `SCENE117_HEIGHT_VALIDATION.json`. Raw evidence is under `work/k06-owner-interface-20261107/SCENE117_HEIGHT_NEIGHBORS*`.

## Source and host identity

This batch adapts original4d3efe15980b633f7ea1a21fee486744cac0c1de then1bb6d8d958a8f9e651ad5612466656f9ead0823e test-substrate hunks across seven module/fixture paths. It adds the original neighbors runner and exact-five height inputs, preserving existing D32capture16 environment isolation and render-loop behavior. It changes no production rendering source or ABI. Coordinator117's ZIP/source results are provenance, not these local results.

The new local host is157524424 bytes, SHA256 `1ad44f2d6045e4a7e29155a730dbb10a8128d279285bf72a51f952dfb55535f3`, on macOS27.0.1 arm64, Apple M3 Pro (Apple9), Metal4.0, Mobile and Forward+. Build base HEAD is828f2f807c83b8674f4ff49edc23e5421d0c83f0; its engine stamp does not claim the uncommitted second-batch adaptation was already in that commit. Build exit0,10.97s. Actual controlled host metadata reports BuiltinGlslangV1, core stamp `5f8994a7744ff8245a48174eebabb1fb4c62957f237d081afdae11dd56023706` and compiler stamp `5b8b179152a3eac3b7e246257acb4008c26e6c2500e2f6d9b32f261532ad9c71`.

Run-pinned height module.cpp SHA256 is `3485b2f254853f8a20a6e600a9620f69c12a7fe20374f7fee2c7462dc8bc5404`; header `f261969655839114174af8f6c65fe7187d1dbb536a761d7c175244397c0e6a20`. Height10 runner SHA256 is `5a5e7b4098a07352b2ccc4e66c3e2430f58c8917b451af7bd4be963f5759c1a5`; neighbors12 runner `7c4e381d1cc2e6ec2cb0607d38271f11a51d40d221b5ebd9b96f926db09534b1`; capture16 runner `21b3f8aef544ee83957308242ae3e27995cbb0129698618cf1e9bf373cdccef9`. Validation retains all21 input entries and bytes.

The before/after input manifests match exactly. Current compiled sources, scripts and host still match those pins. After execution only the height README was corrected to describe four flat inputs plus one tilted plane and fixed per-case starts; its new document hash and run-pinned old hash are recorded under `document_only_after_run`. Submitted documentation is not claimed byte-identical to every run input.

## Actual numeric controls

The original runners require child exit0, exact renderer evidence, no runtime ERROR/WARNING and independent fixed numeric oracles. All logged samples were independently compared with those oracles at absolute tolerance2^-12, relative tolerance0.

Height uses particle world radius.25, frame_counter1, active/collided=true, collider_count1, attractor_count0 and textures_used0. Its five cases each ran on both renderers:

| Height case | Expected normal | Depth | Response position |
|---|---|---:|---|
| identity |(0,1,0)|.125|(0,.25,0)|
| rotation |(-1,0,0)|.125|(-.25,0,0)|
| uniform |(0,1,0)|.125|(0,.25,0)|
| nonuniform |(0,1,0)|.125|(0,.25,0)|
| tilt_nonuniform_rotation |(-.9669875568304565,-.2417468892076141,-.08058229640253803)|.12087344460380706|(-149/616,-9/308,-3/308)|

These are four flat-plane inputs and one tilted local plane `y=.5x+.25z`, with original independent per-case start positions and field transforms. The tilt depth is the legacy local signed-depth oracle, not a newly inferred world sphere-plane response. Actual final tilt depth was.12087345123291; its normal and position matched the fixed expected values. Marker: `HEIGHTFIELD_LEGACY_NATIVE_MATRIX_PASS 10 real Metal GPU cases`.

Neighbors are six isolated ordinary sources ×two renderers, each frame_counter1 and active=true:

| Source | Fixed numeric expectation | Textures used |
|---|---|---:|
| sphere collision |normal(0,1,0), depth.25, position(0,1.25,0)|0|
| box collision |normal(1,0,0), depth.125, position(1.25,0,0)|0|
| SDF collision |normal(1,0,0), depth.125, position(.25,0,0)|1|
| sphere attractor |force(-1,0,0), velocity(-1/60,0,0), position(1,0,0)|0|
| box attractor |force(-1,0,0), velocity(-1/60,0,0), position(1,0,0)|0|
| vector-field attractor |force(-2,0,0), velocity(-2/60,0,0), position(0,0,0)|1|

Collision samples require collided=true/collider_count1/attractor_count0; attractors require collider_count0/attractor_count1 and integrate force for1/60s. Marker: `HEIGHTFIELD_LEGACY_NEIGHBORS_MATRIX_PASS 12 real Metal GPU cases`.

The textured cases are real resources: SDF creates a16³ RF signed-distance image and vector field a2³ RGBF constant(0,.5,.5) image. Original TextureStorage creates 3D textures, verifies valid RD texture/type and excludes default3Dwhite, then binds the actual field texture. After original `_particles_process`, the fixture checks frame source type, texture_index0, exactly one texture and the original texture RID. Actual async full ParticleData readback supplies the numeric result. Cleanup frees particle/material/shader/collision resources before the original texture. These are not CPU-filled results or default-texture fallbacks.

Preserved D32 capture runs eight cases on each renderer: empty; geometry heights-.5,0,.5; adjusted height.25/extents_y2/translation_y-.5; and rotated/rotated-uniform/rotated-nonuniform geometry. Both instance targets are seeded; first_raw remains.75. Empty second_raw is0, ordinary geometry uses `(height-transform_y+extents_y)/(2*extents_y)`, transformed cases expect.75. All16 samples pass at.001 tolerance and cross-renderer equality. Marker: `HEIGHTFIELD_NATIVE_MATRIX_PASS 16 real GPU cases`.

## Cross-regression results and diagnostics

| Current control | Actual result |
|---|---|
| Managed typed-phase/SceneOnly |55/55 per renderer, both exit0|
| Ordinary synchronous Scene pairs |19/19 per renderer, both exit0|
| Original viewport runner |22/22 per renderer, four view comparisons, exit0|
| CPU combined XML |216 cases /90,654 assertions,0 failures;2042 skipped|

Viewport records retain exact RGBA equality,128×128,1300 nonuniform pixels and draws2→2 for top/side on each renderer. They explicitly report `external_scene_connection=not-present` and `pair_membership=not-observed`. Capture file hashes and logged raw-RGBA hashes are distinct identities; no independent visual interpretation is claimed.

All GPU logs have zero literal ERROR/WARNING. Build retains AccessKit/ANGLE setup warnings and the MoltenVK macOS12→11 linker warning. CPU retains three expected negative pipeline-creation ERROR diagnostics, while its XML records0 failed cases/assertions; do not call build/CPU diagnostic-free.

## Historical boundary and exclusions

The prior preliminary legacy8 pass belongs to its old four-case input matrix, radius.1, old module SHA `2588b0f4946f9415bddf4e97c801065ecfbfc8b35e6243c6d53916468ab05500`, runner SHA `e8c91541aa35cb7ba0317f0fc64b47c625fa853fd25b54187618613513f7fe12` and old host SHA `17145c0f3a2b86607045572133cb4e8d4918117592c8d3bca79d744439657a2b`. It is preserved separately in validation. New height10 uses changed starts/names/radius/oracles and tilt; same filename/CLI does not make old8 and new10 the same control. The current10/12/16 passes are fresh local runs, not relabeled8 or source117 results.

This qualifies the isolated ordinary native numeric controls and stated regressions only. It does not qualify connected H25, mixed-source behavior, Full/P01/public connected Scene/pairs, E09 actual operation/scope, K06 production COW/charge/retirement, INSTANCE192/INDEXED144, or B/R01 initializedness/publication/owner acknowledgment. CPU dummy/headless evidence is separate from actual Metal execution. No adoption-commit, push or clean-tree claim is made here.
