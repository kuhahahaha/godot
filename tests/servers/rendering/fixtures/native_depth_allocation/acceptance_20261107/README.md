# Native depth allocation acceptance R1 history — 2026-11-07

Publication held: this is the preserved R1 historical evidence snapshot, not final acceptance for the revised framebuffer owner-reservation contract. Root is adding a distinct typed framebuffer owner-reservation admission field and will build/retest R2 on a new host. R1 did not expose that field. Unknown framebuffer/object/opaque actual bytes remain unknown; no physical zero-byte assertion is made here.

This archive records completed R1 original-source quote/allocation/disposal runs. R1 GREEN host SHA-256: `7e2c5796c813a48b7587fbec6356c0ae9f90f735abf621cf0c6ea8e17b317774` (157641032 bytes). The binary is not included. Original commands, environments, elapsed times, logs and source identities are preserved verbatim.

| Completed evidence | Observed result |
| --- | --- |
| R1 GREEN editor build, tests/module enabled | Exit 0; original build command/log retained |
| R1 focused CPU suite | 55 cases, 6841 assertions, zero failures; 2217 filtered/skipped cases |
| R1 full CPU suite | 2269 cases, 585450 assertions, zero failures; 3 skipped cases |
| Real Metal Mobile pool case, 64×64 | Exit 0; all 29 distinct checks true; requirement/actual 16512 bytes, reservation 16640 bytes, alignment 128 |
| Real Metal Mobile dedicated case, 8192×4096 | Exit 0; all 29 distinct checks true; requirement/reservation/actual 135266304 bytes, alignment 128 |
| Camera propagation neighbor, Mobile + Forward+ | Original runner exit 0, final host identity retained; six original PNGs included |
| Production D32 heightfield clear/sample neighbor | Original runner exit 0, same final host; original output retained |
| Independent source review and quote-test teardown | Review, source pins and exact test-only teardown diff retained; final focused/full runs include the repaired quote case |

Both quote GPU cases identify Metal 4.0, Apple M3 Pro (Apple9), Mobile. They issue original native heap quotes, feed the retained quote into the same-descriptor original factory, observe actual native allocation, record a real D32 clear and materialize original driver framebuffer/render-pass IDs. The five snapshots are LIVE/LIVE, LIVE/ENQUEUED, LIVE/RETURNED, ENQUEUED/RETURNED, RETURNED/RETURNED. Aggregate physical return stays false until both original children are RETURNED. Original identities/backing remain stable; Engine frame count does not advance. Unexposed framebuffer/object/attachment/opaque costs stay typed UnknownNotExposed. R1 GREEN GPU stderr is empty in both cases.

The qualifying controlled RED is **R4 only**: removing the depth-RETURNED requirement for a framebuffer pair. Both real Metal cases exit 1 with exactly `only_framebuffer_driver_returned` false (22 checks reached, error 47). The parent loop independently checks the conjunction, so it rejects the incorrect aggregate while framebuffer has returned and depth remains live. Each failed run preserves its original depth prefix and emits one texture leak warning at shutdown; that warning is disclosed and is not itself the reason this is a targeted RED. Its host hash is `dafe816067c21a2857274f3eb1daf3885470e403704938876d777755b53a5be9`, distinct from final GREEN. The R3 earlier mutation identity and R4 restoration proof are retained. R4 runtime source pins identify the restored GREEN RD source, so they are not the mutant compilation source identity. The distinct R4 build/host/run records and exact target assertion are preserved separately; no equality between R3 mutant hash and R4 runtime source hash is claimed. R4 was restored before the final build and GREEN runs.

`rejected_history/` contains earlier diagnostic material, including the original CPU abort (exit -6) and R2 build/source/restoration records. These are **not qualifying target RED evidence**. The independent reviewer found that a quote-test REQUIRE could unwind through a partially attached stub device; the narrow scope teardown repair retains all original assertions and detaches/cleans original test resources. R1 CPU results verify the repaired source identity. No arbitrary crash, earlier wrong mutation or unrelated tool failure is used to establish the target counterexample.

At this archive checkpoint, viewport/legacy/neighbors were still owned by the parent's active run and were deliberately not read or copied. The parent will append their completed evidence and final hook audit later. Existing hook records are preserved as history, including an invalid `ruff` hook-name invocation; this archive does not claim every final hook is complete.

The R1 observed level is original main-RD/RT native quote, actual-allocation and physical-disposal acceptance plus the listed neighbors. Private RD helpers and this tests-only fixture do **not** establish a production cross-DSO DeviceExit Scene facet, C reservation/closed-record closure, original HeightFrame producer adoption, producer/reader Complete or OutputReleased. Those require their original public ABI/cell/transport/producer adapters. CPU injected-driver branches cover actual-over-reservation and actual-unknown failures; the real GPU cases use real known native bytes and never fabricate witnesses or completion. Other devices, passthrough hazard mode, Vulkan, D3D12 and native OOM are not qualified by these two device runs.

`ORIGINAL_FILE_MAP.json` maps every copied raw artifact to its original absolute path, byte length and SHA-256, plus its stored length/hash. Logs/XML and large source snapshots are gzip compressed with unchanged decompressed bytes. `final_source/` contains all 19 files named by the parent's final source pins; source identity is not by itself binary build provenance. The build/run records provide the separate host identity evidence. The fixture README snapshot is the original pre-run instructions; this acceptance README states the completed level.

`MANIFEST.json` covers the archive payload, including this README and the verifier. `ARCHIVE_VERIFICATION.json` is the independently regenerated verification result and is excluded from its own manifest. Verify without Godot, builds or a GPU:

```sh
python3 verify_archive.py
```

Add `--compare-originals` only when all original workspace files still have their archived revision to compare every copied artifact against its original bytes. The live workspace has advanced for R2, so that optional comparison is expected to refuse changed production files; default verification checks the preserved snapshot independently. The verifier checks all stored/decompressed hashes, the exact archive inventory, final source pins, targeted RED identity/failure, final host identity, both 29-check GPU reports/logs and both raw CPU XML summaries. It reads mixed console/XML reporter output through an in-memory derivation only; raw XML files are preserved unchanged.

## Packaging v2

Original diff payloads use deterministic gzip to retain exact whitespace without Git diff whitespace errors. `packaging_v2/RECEIPT.json` pins each original and stored hash; prior manifests, file maps, verifier and verification records are preserved there as gzip. This changes wrapping and verifier formatting only; source snapshots and raw evidence bytes are preserved. The R1 verifier excludes the appended R2 version directory from its own inventory.
