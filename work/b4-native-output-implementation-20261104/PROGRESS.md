# B4 K native output implementation ledger — 2026-11-04

Execution follows the existing authorized P1–P4 plan in the design master; this is a recovery ledger, not a second decision document.

- P1: strict create-config syntax/projection and checked bytes implemented. Six doctests added; debug/tests, release production, and double syntax checks passed. Doctests not executed. Independent Sol review found no confirmed defect; mesh/material ownership, exact flattened surface count and numeric/profile admission remain storage duties.
- P2: six RS APIs, immutable External mode, native simulator bypass and resource reservation remain pending. No RID/capability is minted by P1.
- P3: Instance192 protected init/validate/finalize guard shader and component fixture implemented; 5 SPIR-V variants (including explicit zero-capacity INIT_EMPTY/FINALIZE_EMPTY), 40 scalar production-control cases and 3 expected-red mutations passed. Guard wrapper prepares five actual pipelines and plans exact descriptors/chunks; preparation has syntax evidence but actual native pipeline/set creation remains unrun. Independent review found and closed the original empty-capacity descriptor mismatch. The wrapper is committed as a6eefb5af7877b798d7a00bbb4cdd586165bc2a5. RD checked production owner integration and both renderer consumers remain pending. Indexed144/SceneOnly support is not inferred from this shader.
- P4: production operation/source association, E09 record-token bridge, no-next-draw maintenance/physical owner ACK and actual E34/U05 route remain pending. Existing source registration alone and qualification ticket mechanics are not production authority.

Latest pass_mask owner decision is imported from OUTPUT_CONTRACT §2 #external-pass-mask: schema1 bits 1/2/4/8/16, KNOWN31; no caller-defined mask policy. Do not numerically cast renderer enums into this wire domain.

Priority milestone received from main: freeze actual dirty System scalar source/ABI before expanding B4. Delivered separately as origin/codex/system-scalar-source-integration-20261104, 46ce3650254884530a332b75245767ccc9c34375 over f7bc2b053d. Source/ABI syntax only; original System pair unchanged; three runtime gaps documented there. Complex retention/capacity fixes proceed in separate sparse worktree, never mixed into this branch.

No build, live GPU, renderer adoption, valid producer token or whole B4 acceptance is claimed by this ledger. Long goal remains active.

- P4 component landed: private Core storage/uniform physical-disposal witnesses committed as 20f6476ecb4baaf25c515a8e48bda884fe7b8a5f, independently reviewed; old-return-site red2/current green0/premature publication red3, three syntax passes. Four engine doctests are not run. Actual driver return must precede RETURNED and mismatch retains UNKNOWN. This is mechanical disposal only; original E09 authority, all recorded uses and owner ACK are still required.
- Current shader component: Instance192 dual-RD shader/compiler variants, spatial readonly inverse builtin and GLES compatibility mapping. Astra independent review found and closed Native trails global-group invalidation and verified the existing uber dynamic-cull interface. Current source/template review has no unresolved component blocker; actual Engine group lifecycle, External draw/adoption and GPU acceptance remain unrun. Exact final source hashes and raw compiler checks are preserved in the independent review package.
- Existing external scons processes are still running; no B4 build was started. B4 tree is now fully materialized for its eventual isolated build. The frozen a2 System source tree and 4e99 host remain unchanged.

Next isolated build/test queue (not executed; wait for existing builds to finish):

- Build only the B4 tree after the shader review closes: `scons -Q tests=yes platform=macos target=editor -j2`. Record final source revision and all dirty-source hashes before invoking; preserve the old System pair.
- Discover then run `[NativeDisposal]` (4 cases), `[ExternalOutputGuard]` (5), `[ExternalOutputConfig]` (6) and the two `inverse model` cases. Reject zero matches or missing force-link entries; do not infer GPU behavior from these mock/pure engine tests.
- Run adjacent rendering/ShaderLanguage tests appropriate to the final shader changes. Actual shader compiler/native GPU preparation, per-entry consumer and full E34/source/adoption/retirement remain separate gates.

Shader package staging verified all eleven production hashes against the final Astra review manifest. Production/test `git diff --cached --check -- servers drivers tests` is clean. Expanded GLSL evidence retains the exact compiler input bytes, including final blank lines reported by the all-artifact whitespace check; these raw snapshots are not normalized after hashing/compilation. No compiled binaries, SPIR-V outputs, Python caches or project caches are staged.
