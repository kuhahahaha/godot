# External packets 203–222 source review

All eight packets are **received / reviewed / not adopted**. The source package contains an actual applicable patch. This review confirms its bytes, hunks, reconstructed source and interface scope; it does not adopt source or certify production integration.

## Package and tree evidence

- Packet 206: 4094 bytes, SHA256 `3d8cee930bbac155e32f4b753fe84edf347618b1513bd154a782ef9355f6f994`.
- Packet 209: 16603 bytes, SHA256 `c262ffae54b4fbe61b6f2687886ce40978c4ef461f1cca6beee93539ca3d5544`.
- `adapter-and-probe.patch`: 51348 bytes, SHA256 `bcc079bc3aa0f95d6355c33a392c9287b53a2745035fbe56b6ad651dbe668e83`.
- Independent temporary `GIT_INDEX_FILE`: `read-tree f62766d37c561d59c0b7949cd73fa2003015db77`, cached patch application, `write-tree` produced `817cf0104cff150e1506b23e69b0d5dd67317458`, exactly the tree claimed in 220. No working-index or checkout application occurred.
- Initial authorized exact HTTPS fetch failed with `LibreSSL SSL_connect: SSL_ERROR_SYSCALL`. The parent subsequently fetched the exact commit successfully. Independent local `rev-parse 2eb58a826d17b021eebc682b41ec55a9eba2153c^{tree}` now confirms `817cf0104cff150e1506b23e69b0d5dd67317458`. The patch-to-tree and commit-to-tree relationships are both independently verified.

The four common logs/stat files in 206 and 209 are byte-identical. Their receipts differ; 209 adds explicit neighbor-log identity. `source.diff` remains a 600-byte stat. The separate `adapter-and-probe.patch` supplies real hunks; confusing these files would incorrectly dismiss 209. Full packet, package-member and reconstructed blob hashes are in the companion JSON.

## Actual source and interface findings

The reconstructed tree differs from reviewed `8c6f2fb72af1e9152beeaa3d45dba5afcaa88d1b` in four files, 65 additions and 6 deletions: fixture cpp/h, one TESTS-only RenderingDevice friend, and `run_resources.py`. All previously reviewed allocator, quote/projection, HeightFrameResources and checked-sink implementations are unchanged. Prior retry, prefix preservation, refcount and original-owner restrictions still apply.

The fixture now obtains an original native quote, passes each original owner Ref by reference to `allocate_quoted`, checks CreatedVerified/known actual bytes within reservation/LIVE originals, and passes those same owner objects to `begin_quoted_target`. Retirement checks projection return and matching original serials. The failure-cut case retains the second original owner. The runner's `--quoted` is a Python runner argument: it sets `ANGE_HEIGHTFIELD_QUOTED_RESOURCES=1` in the child environment. Therefore its absence from the printed engine COMMAND array is explained by actual source. The runner requires both quoted result flags.

All new quoted allocation/sink callers are in the test fixture. Production HeightFrame/renderer capture sites were not changed to these calls. No Bridge publication, authenticated full NativeRecord/Transfer/cleanup enrollment, producer/reader membership or last-use integration was added. Existing private quote API signatures need no duplicate implementation in D.

## Log identity and limits

`quoted-fixed-gpu.log` reports Mobile and Forward+ Metal cases with version `4.7.2.stable.custom_build.2eb58a826`, quoted allocation/projection flags true and final quoted matrix 2/2. This is source-reported adapter probe evidence. The build log reports successful linking, but neither package includes the reported binary for independent hashing. Binary bytes 156269320 and SHA256 `2b9e22cb47f4fe55827b71b6d99dd19c5a2af550a89a187a58359a6ea99634e5` remain receipt/source reports. No local build, engine run or GPU reproduction was performed.

`refusal-after-quote.log` has an `f62766d37` banner. It is precommit geometry-refusal neighbor evidence, not fixed quoted proof or geometry enrollment. Packet 203 correctly limits the earlier attached 8c6f evidence to precommit f627 builds and legacy unquoted paths. A quoted 2/2 pass does not establish D public-five/K01 Complete, C E05/U08 claim, positive multiinstance or whole FR.

## Business disposition by packet

| Packet | Status | Finding |
|---|---|---|
| 203 | received / reviewed / not adopted | Reviewed correction: old build/GPU banner f627; unquoted neighbor only. 8c6f source review remains unadopted. |
| 206 | received / reviewed / not adopted | Verified 4094-byte evidence package; fixed-source/binary identities and quoted GPU 2/2 are source-reported, not independently replayed. |
| 209 | received / reviewed / not adopted | Verified 16603-byte source package and actual 51348-byte patch; inspected reconstructed source, interfaces and fixture caller. |
| 212 | received / reviewed / not adopted | Reviewed follow-up; actual source patch reviewed here, no repeat engine matrix and no adoption. |
| 214 | received / reviewed / not adopted | Reviewed request; technical byte-count/ancestry statements superseded by 217. |
| 217 | received / reviewed / not adopted | Reviewed correction; independently checked ea244 has 2249 additions/9 deletions/11 files and depth-clear-only signature. |
| 220 | received / reviewed / not adopted | Independently applied patch with temporary GIT_INDEX_FILE to f627 and reproduced claimed tree; independently verified fetched local 2eb58 commit tree equals it. |
| 222 | received / reviewed / not adopted | Answered B per original D owner: requested authenticated geometry prefix enrollment and production caller do not exist; D owns future integration; no fixed successor SHA or ETA. |

Packet 217 supersedes 214: ea244 has 2249 added lines, 9 deleted lines, 11 files, not 2249 bytes. Cherry-pick does not automatically import ancestry. The actual dependency issue concerns missing matching frame/admission state and hooks such as `qualification_graph_closed`. Independent source inspection confirms ea244 exposes `_native_height_range_record_depth_clear(range, depth witness, framebuffer witness, clear_depth)`, which does not accept Scene geometry checked-sink records or producer/reader membership.

For packet 222, the original D owner's substantive answer is **B**: the authenticated full geometry NativeRecord/Transfer/cleanup-prefix enrollment and production caller do not exist. **D owns future integration. No next fixed SHA and no ETA are available.** This is an explicit interface absence, not a transport acknowledgement or indefinite source-review status. No outgoing external message is made by this review.
