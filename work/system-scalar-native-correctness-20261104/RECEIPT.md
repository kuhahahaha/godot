# System scalar native correctness — matched source/host/SDK

Verified on this Mac with Metal4.0 and Vulkan/MoltenVK. This is the named System native component package; it is not ANGE Host, whole System/U1/U05/lifecycle/phase1, B4 renderer output, or all-product acceptance. The long Godot goal remains active.

## Exact consumption pair

- Built source: `a2f009e6cd5251abea5a611eaf9edac50b8749b3`, fetchable through `origin/codex/system-scalar-correctness-20261104` (later commits only package validation tooling/evidence).
- Host: `host/editor.arm64`, SHA256 `4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528`,148666568 bytes.
- Adjacent controlled metadata SHA256 `1286dd4abfdc667c52d028790925b2a87bfee4d726a6fe39d649eed7ff824530`.
- SDK: `sdk/gdextension_interface.gen.h`, SHA256 `ec25549317a591c3009de34f217bcb835f1d3cc52ba2d8782bde2af15d5dbad7`.
- Closed selection: `CoreBuildSelection.schema1.json`, SHA256 `5ba0059df4861c2c9b4edead3cc9fa683eac7308a2e92e0cf336cd541bfd555a`.
- Core stamp `b9cdbb652249045e525f1276dc78869a134ff8e03f9d8082a37a5c4efb173d0c`; compiler route `BuiltinGlslangV1`, compiler stamp `61913a1a11b3f57d6a1ac8efa90c760b01d03ce819d4e19a0e2e845753edad0c`; macos/arm64/Executable.

The frozen local host is `/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64`. It is retained locally, not committed as a150MB Git blob. PAIR_MANIFEST.json binds all six files. The shared original System1da pair remains untouched; the previous Vulkanb173 immutable copy was also checked unchanged. Consumers must prepare against this new selection rather than reuse old-core containers.

## Source and build

The complete source/ABI closure is46ce365025 (correcting the previous inaccurate System-capable description of Vulkan-onlyf7bc2b053d). Fix1cb962fb3a retains Unknown append obligations and performs native/fact capacity admission before ticket/serial/set/append. Fix3da5cd3d18 surfaces persistent internal-set retirement failure through the original Error channel without rewriting completed state8/report/revision/native code. TESTS-only, System-only, once-per-ticket controls area2f009e6cd. ImageTexture include83abcb982c and existing CurveXYZ test access make the clean test composition buildable without foreign dirty files.

One isolated builder ran `scons -Q tests=yes platform=macos target=editor -j2`, exit0,1025.961s. The current SDK27.2 caused ordinary recompilation from a27.0 cache. Source/head hashes were checked before and after; SDK was taken from the actual generated build. Raw build.log, BUILD_SOURCE.json, BUILD_RESULT.json and BUILD_IDENTITY.json are retained. The existing MoltenVK minmacOS12 versus link-target11 warning is preserved; this receipt makes no deployment/platform claim beyond the observed machine.

## Engine checks

| Selection | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| SystemScalar | 7 | 50 | exit0 |
| RecordSource + QualificationRt | 15 | 717 | exit0 |
| VulkanQualification | 2 | 25 | exit0 |
| Qualification full integration regression | 209 | 44101 | exit0 |

Selections overlap; do not sum unique cases. Expected refusal shader diagnostics remain in the209-case raw log. These are headless engine tests, not GPU evidence.

## Actual native cases

COVERAGE_INDEX.json maps the ten named observations to exact result rows, inputs and hashes. Metal reuses the already-green positive row from021819 while retaining that suite's later close failure as failed; the corrected close and three negatives were run individually. Vulkan ran the complete five-case suite once.

| Case | Metal | Vulkan | Proven boundary |
| --- | --- | --- | --- |
| positive | pass | pass | actual backend/Engine identity, exact3.0 (`0x40400000`,4B), native completion, physical free and owner ACK |
| post-submit close | pass | pass | sealed Body non-MAX command slot and exact native tuple before close; same original source/Body through completion, physical free/ACK |
| native capacity | pass | pass | actual current0/workers11,G513,F514→6168 slots>4096; error5/reason19, zero ticket/report, then same-context small request completion/free/ACK |
| append observation Unknown | pass | pass | TESTS injection after a real graph append, actual original ticket remainsUnknown/retained, copy/release/bridge close refused; explicit process isolation |
| internal set retirement error | pass | pass | TESTS maintenance-seam failure; original state8/revision/report and exact3.0 copy remain, observe/release return sticky error, bridge close refused; explicit process isolation |

The capacity refusal is the shared pre-driver source-slot gate, not an actual GPU/device error. Unknown is an adapter-observation injection, not a claim that the hardware returned an unknown result. Set error is a maintenance-seam injection, not a native driver failure. Those two controls deliberately use `_Exit(0)` only after successful retention assertions; `retirement_claim=false`, no false DSO unload/free/ACK. Process exit is not native completion.

Metal's close observation had completed state8 and raw successful native status4. Vulkan observed state4/native issue1/completion1 before close. Both preserve exact Body/source identities at final return. No still-executing-GPU/inflight claim follows from a sampled observation.

## Retained failures and fixture repairs

All original failures and their input snapshots remain unchanged. The first generator failure was a fixture Dictionary equality issue: Engine schema is INT, JSON parses it as FLOAT, so whole Dictionary comparison rejected identical scalar fields. A closed seven-field matcher now requires Engine INT1, JSON exact1, and exact typed string identities;54 actual-host GDScript checks pass. Identity is emitted in `_init` before synchronous SceneReady, so isolated negative exits cannot bypass actual identity checks. Global fatal checks cannot be disabled by a negative-case configuration.

The first close predicate incorrectly required possible_count0 during every intermediate observation. Real Metal hidden-copy source issue2/completion1 may remain pending while reportstate2; only explicitly matching original rows are now accepted as pending. State5/6/7, real Error and failed/Unknown completion remain rejected. A hidden preparation copy alone cannot satisfy Body submission. A second fixture error treated successful Metal raw native_code4 as Error; original raw diagnostics are now retained while the original issue/completion/Error fields decide success. Final matching covers original pending sources, native tuple and full Body graph identity. Source code and the frozen engine host were never changed to accommodate these fixture failures.

## Integrity and review

All9 resultsets, including failed ones, were freshly checked:348 retained artifacts have matching bytes/SHA; every historical input snapshot and bound host/SDK/selection matches. No issue found. Independent Sol review separately checked259 artifacts in the six selected resultsets and source/build/pair/test-log identities, then read the actual native logs and scope. The coordinator independently checked244 final-control artifacts and queried the cloned host's real Engine identity. Counts differ by the explicitly selected directories, not conflicting verification.

The new pair is available for A/B consumption and separate integration acceptance. This receipt does not replace their ANGE Host/U1 qualification, does not turn qualification/source tags into production authority, and does not close B4's source bridge, renderer consumption/adoption or retirement queue.

The coordinator has finitely accepted this component after a separate non-author source/raw audit (`FINAL_NATIVE_SOURCE_REVIEW.md`) and independent input/artifact checks. This does not extend the scope above. No further GPU or unchanged-fixture rerun was performed for packaging.
