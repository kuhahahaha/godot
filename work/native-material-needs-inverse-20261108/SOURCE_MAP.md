# Original native material inverse requirement source map

Candidate-only investigation. No live or B file edited; no build. B contract reads were observational and concurrent B changes are not frozen by this note.

## Exact meaning and original consumers

SpriteTransform input word 1 is `inverse_required` (integer 0/1), independent from the normal rows always produced by that checked source. See ANGE `work/e06-source-native-successor-20261106/source/E06_SOURCE_JOIN_REQUEST.md:21` and `work/e06-whole-source-unit-current/STAGE_HANDOFF.md:26`. Optional inverse absence has zero inverse rows. Therefore a material that consumes inverse model data must not receive an unconditional zero requirement.

Godot's Instance192 shader reads rows 6–8 as `inverse_model` in `servers/rendering/renderer_rd/shaders/instance192_inc.glsl:55`; rows 3–5 are separate normal data. The forward shaders use inverse model in two semantic features:

| Original feature | Clustered | Mobile |
|---|---|---|
| `INV_MODEL_MATRIX` in material shader, vertex stage | `scene_forward_clustered.glsl:384–386` | `scene_forward_mobile.glsl:436–438` |
| `INV_MODEL_MATRIX` in material shader, fragment stage | `scene_forward_clustered.glsl:1466–1468` | `scene_forward_mobile.glsl:1361–1363` |
| alpha hash object-space position | `scene_forward_clustered.glsl:1541–1543` | `scene_forward_mobile.glsl:1495–1497` |

Paths in that table are under `servers/rendering/renderer_rd/shaders/forward_clustered/` or `forward_mobile/`. The unconditional local alias at Clustered:288/Mobile:339 and record decoding do not by themselves prove material semantic use. Normal transformation is independent and must not be used as an inverse-demand substitute.

## Compiler provenance

`SceneShaderForwardClustered::ShaderData::set_code` and Mobile's equivalent provide `ShaderCompiler::IdentifierActions` to the original spatial compiler. Existing default actions map `INV_MODEL_MATRIX` to `INV_MODEL_MATRIX_USED` (Clustered cpp:927, Mobile:882); `ALPHA_HASH_SCALE` maps to `ALPHA_HASH_USED` (Clustered:912, Mobile:867). `ShaderCompiler::_dump_node_code` sets `usage_flag_pointers` beside the same usage define discovery (`servers/rendering/shader_compiler.cpp:915–925` and `1049–1059`). This is the precise original hook for separately tracking these two requirements, without text searching generated shader code or guessing from Material classes/uniforms.

Existing `uses_alpha_clip` combines `ALPHA_SCISSOR_THRESHOLD` and `ALPHA_HASH_SCALE` (Clustered:134–135, Mobile:135–136), so it loses exact hash distinction. `uses_normal`, `uses_tangent`, `unshaded` do not answer inverse use. Existing `instance192_shader_supported()` (Clustered:538, Mobile:505) is explicitly structural support only, not original material/profile/history/displacement qualification.

A minimal compiler change would collect local `uses_inverse_model_matrix` and `uses_alpha_hash` flags in `set_code`; preserve the existing alpha-clip action through its existing flag and separately derive hash from a dedicated new action flag (the hash pointer cannot replace alpha-clip without preserving its existing semantics). Commit the requirement to ShaderData only after successful original compiler return. Invalidate any exposed fact before empty-code and failed-compile exits. For this profile, inverse required is the OR of these exact two semantic uses. Future consumers must extend that source-owned predicate when a new shader branch uses inverse rows.

## Existing owner boundaries and missing issuance

B `RenderMaterialContractRef` retains original Godot Material/dependencies, and `require_native_qualification()` returns CAPABILITY. Its current candidate `needs_inverse()` also returns CAPABILITY (`render.material.native_needs_inverse_evidence`). `RetainedRendererResourcesLease::require_ready()` calls each original material's qualification. This work does not change B readiness or invent generation evidence.

Godot `MaterialStorage::NativeSelectionLease` (`material_storage.h:115–128`) is the existing bounded original material/shader CPU selection retention. Its acquire/release checks original render thread, material generation and owner, and increments/decrements material and shader selection holds. `material_get_native_preparation_data` (`material_storage.cpp:2960`) checks exact material generation, actual 3D material/shader/data and clean serviced state; it does not issue feature/profile qualification. `ShaderRD::version_native_shader_is_ready` authenticates ready shader membership only. Native material hold acquisition authenticates actual shader/pipeline/set layout after those objects exist. None of these currently returns a typed inverse requirement to B.

Live external output Create explicitly refuses missing original parent/source/material/qualification issuer (`particles_storage_external.cpp:148`). Installed output DTOs carry material RIDs but no original material qualification fact. No existing live material-qualification export or native `needs_inverse` fact was found. The exact original D bounded qualification receiver candidate must be identified before extending that receiver; adding a second registry or admitting render-profile Create from a compiler boolean would be incorrect.

## Narrow receiver shape to implement in the original owner

The bounded receiver should return an owner-authenticated material feature fact tied to its existing retained selection: original material RID + exact generation, backend/profile, original ShaderData/ShaderRD version identity, and a closed inverse-demand enum (unused/required). Caller-authored booleans or public POD fields cannot issue it. The owner validates the original selection lease and current material/shader association before reading compiler-owned demand; no compile, RID allocation or new global registry is needed. The fact remains a feature constituent. Full material/profile qualification must additionally retain its original displacement/bounds, history, backend/cull and readiness evidence.

B may consume the inverse-demand projection only from that existing qualified material owner; it must continue returning CAPABILITY when that original evidence is absent. Aggregate surface use requires OR over the actual retained surface materials (not a substitute MaterialRegistry enumeration). C then passes the resulting 0/1 to original SpriteTransform arg1. This note does not define or authorize a parallel caller-owned qualification object.

## Verification needed after receiver ownership is located

Original compiler fixtures should distinguish plain material false, explicit vertex/fragment INV_MODEL_MATRIX true, ALPHA_HASH_SCALE true, alpha scissor without hash false, normal-only false, failed/empty shader no fact. Owner tests should reject foreign lease, wrong material generation, shader replacement/failed compilation, stale released selection, unsupported backend/profile; the fact must not grant full material qualification. B/C integration must preserve explicit unavailable behavior and pass the exact aggregate demand to SpriteTransform rather than constant zero.
