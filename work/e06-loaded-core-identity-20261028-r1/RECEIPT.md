# E06 loaded controlled core identity — executable delivery, 2026-10-28

Status: macOS arm64 executable, shared-library and static aggregate producers/getters implemented and verified. Nonbuiltin/unsupported linker branches remain unfinished. This is not full E06, adapter/SDK/target assembly, U08/Ready/G1 or long-goal completion. Root-adopted main contract remains the unique design: docs/contracts/particle/e06/CONTROLLED_BUILD_IDENTITY_CONTRACT.md in ANGEParticleDesign.

## Fixed consumer handoff

- Direct closed schema1 record and original producer Node: repository core/engine_build_selection.gen.json. Frozen actual consumer input: fixed-inputs/CoreBuildSelection.schema1.json (seven fields, no wrapper/fallback/artifact metadata).
- Matching executable: fixed-inputs/godot.controlled.exe.arm64; SHA256 d216693315918595bf7cf46d57006d88ede1d7000fe3cd68fac06afb63edffac,302573616 bytes.
- Separate final artifact receipt: fixed-inputs/CoreArtifactReceipt.json. Debug input bundle: fixed-inputs/CoreBuildInputs.schema1.json. GeneratedStamp.cpp is audit source only, never an alternate producer.
- Actual loaded core stamp294b57e8cbf2e8b355e27dd435624a463d55f0639e6a4caab482c964d49c4409; compiler stamp87a95f076f5f016beb0d68bea7178ba00c54885a99e4492e75f5b3525bc4980c, route BuiltinGlslangV1, target Executable.
- Existing canonical882d is preserved byte-exact/executable at fixed-inputs/godot.882d69ab.arm64. Prior K05 actual Metal proof remains on its original private host; no historical receipt was silently repointed. Source is a mixed worktree, not clean-checkout reproduction; foreign Metal SCsub flag is pinned through actual object/archive input, not adopted here.

## Original acyclic implementation

Original validated SCons options feed the canonical per-option policy. Original macOS final Program/SharedLibrary objects and expanded original clang→ld command produce ordered full-content inputs, including implicit libc++/System/compiler-rt/LTO and real selected framework stubs/libraries. Both original node children and its Executor sources receive one generated stamp object. Every original source is represented; only generated stamp object/final target are excluded. Stable hand-written header contains declarations/types only; Engine.o cannot include a generated value. Dictionary comes from the actual loaded constant, no caller digest/path/OS-executable lookup, new registry/service/ClassDB class or runtime file read.

Snapshot schema1 uses C01 BE scalars, counted UTF8/ordered arrays, raw32 digests and exact ordered duplicate link inputs; compiler domain binds full core stamp+route1. Tools bind original action ENV-selected independent CC/CXX/link driver/actualld and selected dispatch binaries. Actual SDKSettings, tool versions/configuration and generator/recipe content participate. Jobs/naming/post-link diagnostic options are excluded; selected SDK/tool/module paths are represented by actual tool/SDK/link/object content; profile file selection binds content+dependency, runtime system_certs_path remains semantic. Typed argument tags distinguish literal/Input/Output/Tool/Framework/SDK/DefaultInstallName; implicit dylib ID derived from -o remains semantic. Supported parser grammar has explicit arity; unknown/search-changing/indirect-file modes cannot publish a supported stamp.

Separate direct selection and debug bundle are generated alongside stamp source. Final artifact bytes/SHA are recorded after existing target debug/strip post-actions; separately generated/signed bundles are different artifacts. The recipe file itself participates; edits to receipt FUNCTION CODE in that same file conservatively restamp. No receipt-code-only or bundle-signing experiment is claimed. Actual no-op preserves the semantic stamp/object/link.

## Verification

- FINAL_GENERATOR:19 checks rc0; C01 independent framing, content/order/duplicate/options/tool/recipe invalidation, path relocation, actual collected diagnostic/profile policy, original construction ENV process tests, separate CC, semantic install-name/rpath, unsupported modes, cycle/omission rejection.
- FINAL_CANONICAL_BUILD_R2 rc0, FINAL_NOOP_BUILD rc0; no-op explicitly reports '.'up to date with no compile/link. Actual canonical build includes corrected header, original core archive, generated stamp compilation and actual final link/postaction.
- FINAL_CLOSURE_AUDIT:134 actual input/tool/recipe content records independently reread, all match;121 ordered link inputs contain every original source and no stamp object/final executable. Saved snapshot recomputation matches separate direct selection.
- FINAL_CANONICAL_RUNTIME and FINAL_RELOCATED_RUNTIME rc0: actual Engine.get_version_info returns exact closed7 fields/readonly true/original version fields retained. Byte-identical executable copy at another path reads same loaded constants. Headless here proves CPU/module metadata only, not GPU.
- FINAL_CANONICAL_NATIVE:30 cases/6111 assertions rc0 for B4, viewport history and ShaderCompilePolicy. Three foreign-camera errors are intended negative cases. This also integrates current K05 native CPU source into canonical; no new whole GPU/cancellation matrix claimed.
- FINAL_RUNTIME_INPUTS_BEFORE/AFTER:18 pins identical, independently recomputed by reviewer. Final stable header50lines, no accidental Engine class.
- Independent source+evidence review /root/indexed144_native_review: initial findings reproduced/fixed; final no unresolved concreteP1/P2 in this bounded route. No SDK/product builder or external message sent.

## Failed prototypes retained honestly

RED_GENERATOR is missing-source scaffold, not behavioral proof. CANONICAL_BUILD rc2 genuinely detected stamp omitted from Executor. Initial runtime expected JSONschema float vs nativeint mismatch was a fixture comparison error, fixed only in test expectation; INITIAL_JSON_TYPE_PROBE preserves it. Earlier relocation copy lacked execution mode; corrected with copy2. NOOP_BUILD relinked because callback captured mutable SCons Node; immutable path closures fixed and FINAL_NOOP proves stable. FINAL_CANONICAL_BUILD rc2 detected accidental Engine header copy during license-prefix formatting; corrected header then FINAL_CANONICAL_BUILD_R2 succeeds. These logs are not accepted GREEN or hidden.

## Remaining applicable branches and continuing queue

Shared actual build/load/getter is sealed in work/e06-loaded-core-shared-20261028-r1. Static now uses target-specific generated stamp/selection nodes and an original libtool aggregate over the platform archive plus ordered env.LIBS archives; nonbuiltin runtime compiler and unsupported native ld search/indirect/unknown inputs still fail closed before publication. These remaining branches are unfinished and do not block editor/Metal first-chain integration.

A owns selected SDK Node projection, adapter original build/expected-core selection and normal Host/loader target assembler. A can consume the fixed direct record with matching fixed executable now; no SDK rebuild is needed here. Genuine profile/capability/session/U08/E05/E09 joins remain independent; metadata grants no admission. Continue actual shared validation, static closure work, K04six original output ports/full runtime B4 matrix, and E20 typed T1 after root adoption.

## Static aggregate delivery

The earlier graph audit and temporary prototype identified the missing archive closure and generated-node collision; both are now resolved in production source. `platform/macos/SCsub` builds a platform-only intermediate archive (`libgodot.platform`) and `core/engine_build_identity.py` emits static-specific `engine_build_identity.static.gen.{cpp,json}` and `engine_build_selection.static.gen.json`. The aggregate target uses the original selected ordered `.a` Nodes plus the stamp object and invokes macOS `libtool -static`; the final receipt attaches to the aggregate target, leaving executable/shared generated nodes untouched.

- `STATIC_BUILD_R9.rc=0`, repeat `STATIC_BUILD_R10.rc=0`; aggregate: `bin/libgodot.macos.editor.dev.arm64.a`, 3,258,828,936 bytes, SHA256 `add6c7333a10aeda308ce7d58c4babeb72b08be674b38deca7820fb9817261a5`. The aggregate uses Apple `libtool -D` reproducible archive mode; the two consecutive builds produced the same SHA.
- `STATIC_ARCHIVE_CHECK_R7.json`: `ar -t` rc0, 3,128 members, no nested `.a`; `nm` rc0 with `get_engine_build_identity`, `libgodot_create_godot_instance`, and `libgodot_destroy_godot_instance`. ICU no-symbol member warnings are retained as linker diagnostics. The check is paired with the current schema1 selection (`core 3055b11c59b8720226b4924764729ee619d27f77abfb45e759a0857399635801`, compiler `fb4e6b20bc54eafb4d258307fe4490232dcff785ac1c9fb1ccd44c76d87f7ad3`).
- `STATIC_HOST_BUILD_R5.rc=0`, `STATIC_HOST_RUN_R8.rc=0`: host links only the current aggregate archive plus actual system frameworks/MoltenVK/system libs, starts/iterates original `GodotInstance`, reads exact static selection through original `Engine.get_version_info`, and destroys cleanly. Loaded values match the current archive: core `3055b11c59b8720226b4924764729ee619d27f77abfb45e759a0857399635801`, compiler `fb4e6b20bc54eafb4d258307fe4490232dcff785ac1c9fb1ccd44c76d87f7ad3`.

This proves the macOS arm64 static producer/getter and original C ABI lifecycle. It does not prove adapter/SDK integration, U08/Ready/G1, GPU execution/readback or physical retirement.
