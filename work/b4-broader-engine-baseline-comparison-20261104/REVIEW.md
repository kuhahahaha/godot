# Broader engine diagnostic baseline and negative-case attribution

Limited conclusion: **no additional diagnostic family, payload or count under the exact requested filter**. All8 ERROR messages and the one malformed-UTF warning are independently attributed to five intentional negative tests on both hosts. There are no unexplained messages in this observed set. The original new-host execute verdict remains `pass:false` because stderr is nonempty; this report does not relabel it a clean process or suppress any output.

No test/production source was changed, no engine build was started, and no further host is needed for this attribution. This is a finite doctest comparison, not full-engine rendering/GPU or whole-B4 acceptance.

## Hosts and full-filter observations

Filter, unchanged: `*RenderingDevice*,*ShaderLanguage*,*Qualification*,*B4*,*ShaderCompile*,*ShaderPreprocessor*`.

| Identity | Fixed host SHA256 | Cases/assertions | Result |
| --- | --- | --- | --- |
| Old a2f009e6cd5251abea5a611eaf9edac50b8749b3 | 4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528 | 252 /50356 | rc0, XML all pass,8 ERROR+1 UTF warning |
| New94b06dc4876d153492aa0c0e2f3457d2f5afc92e | 965cb0ef6e137dcf17298d504f404a6f53aaa0421d03d080ef36e9b5b9b24eb6 | 258 /50430 | root's rc0/XML all-pass observation, same8 ERROR+1 warning, strict pass remains false |

Old-source run is `runs/20261104T052814Z-old-a2`; it is never labeled source94b. Root's new-source raw observations stay in sibling `work/b4-broader-engine-recheck-20261104/`. `COMPARISON.json` records exact added names: the four NativeDisposal and two inverse-model tests, accounting for6 cases/74 assertions. No old case disappears.

After excluding only source-site lines, the9 diagnostic payload lines match byte-for-byte and in order. The shader/pipeline error source line numbers move with their containing source; no diagnostic text is generalized away. All original stdout/stderr/XML are retained. `EXTERNAL_INPUTS.json` binds the root-owned new observations without copying over or modifying them.

Baseline equality alone was not accepted as evidence of intent. Each candidate was then run separately on both fixed hosts, selecting exactly one test (and the specific malformed-UTF subcase where applicable). All10 executions have rc0, exactly one XML test and zero assertion failures; each emits exactly the expected message family/count below. Complete commands, assertions and diagnostic payloads are in `runs/20261104T053021Z-single-case-attribution/RESULTS.json`.

## Exact negative-case attribution

| Negative case | Diagnostic count on each host | Assertions in isolated process | Source expectation and effect boundary |
| --- | --- | --- | --- |
| `[Qualification][T1] marked texture rejects driver callbacks before an ordinary prefix` | 2 texture qualification-return /ERR_UNAUTHORIZED | 10 | Loops both without/with an ordinary buffer prefix. Each call explicitly checks ERR_UNAUTHORIZED, no driver callback in the graph and no ordinary buffer tracker mutation. The texture scan refuses before callback effects. |
| `[Qualification][ChildCapacity] partial native unit returns its exact shader before cold archival` | 2 null pipeline-driver ID | 49 | Fake driver deliberately fails pipeline creation for two physical units. The test expects a **partial** result, state2/known_mask1, retaining its actual shader prefix; it then retires/ACKs that prefix and checks references return. This is not a zero-effects rejection, and is not represented as one. |
| `[Qualification][ChildCapacity] definitive empty unit failure returns its history credit` | 1 null shader-driver ID | 57 | Fake driver deliberately fails the first shader create. Explicit checks require FAILED, zero backing handle, no published origin/child and unchanged private-reference count. It then clears the fault and exercises recovery. A driver attempt occurs; no backing is published from the failed attempt. |
| `policy_container [ShaderCompilePolicy]`, subcase `Noncanonical and malformed UTF-8 names are rejected before reserialization` | 1 invalid UTF-8 leading byteff | 2 | Supplies BOM and literalff names; both serialized inputs are explicitly rejected by CHECK_FALSE(from_bytes). The visible warning comes from the encoder's TESTS audit converting its rawff name through String::utf8 **before** ERR_PRINT_OFF; decoder rejection printing is already disabled by the test. It is input rendering, not successful normalization or native backend compilation. |
| `[B4][E15] viewport camera source requires explicit C01 registration` | 3 foreign-camera-token | 38 | Two foreign token/data calls and one new token paired with old captured camera data deliberately fail the viewport guard. Subsequent assertions verify previous camera data/token remain absent or preserve the original admitted tuple. No new camera history is accepted from those calls. |

The three test source files containing these cases are byte-identical between a2 and94b (hashes in COMPARISON). Line-numbered supporting excerpts are in `SOURCE_EXCERPTS.json`. In particular, codec rejection is explicitly checked; the auxiliary UTF warning is not treated as proof of rejection by itself. The partial shader-prefix test is likewise distinguished from an effect-free refusal.

## Remaining limits and integrity

There are **no pending unexplained diagnostic payloads among these9 observed messages**. This does not classify diagnostics outside the exact filter, certify every backend or convert a nonempty stderr stream into a clean run. Root's original broader execute `pass:false`, old-host source identity, native-disposal shutdown failure from the older159fcc host, and all previous evidence remain unchanged.

Final auditing found that the isolation script had included its previous FILES.json as an input when regenerating that manifest after the second host. Only that stale self-entry mismatched. The original manifest and original script are preserved; the regenerated manifest excludes itself and verifies every entry. `MANIFEST_CORRECTION.json` records this metadata correction. No raw log/XML/result/verdict was changed to fix it. Package hashes cover both the retained original and corrected metadata.
