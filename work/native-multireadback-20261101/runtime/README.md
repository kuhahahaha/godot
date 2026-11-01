# Original Metal V2 multi-readback native control

Status at handoff: **both standalone probe dylibs compiled; no Godot execution or container generation performed by this worker**. Main coordination must run this against its new source-matched executable and controlled-build metadata after integrating V2. This is a Godot native ABI/control fixture. It is not an ANGE product request, original R01/R02 acceptance, provider grant, Method verification, or owner acceptance.

The fixture copies the established guard/SceneReady/main-session/QRT/physical-return scaffold from `work/native-bridge-implementation-20261026/integration-preflight-20261026/native-fixtures/qrt_upload_probe.cpp`. The original fixture is unchanged. Everything added here stays under this runtime directory; no production source or global engine artifact is changed.

## Exact payload and expected bytes

The actual original main session creates four E05 storage children, with allocation slot 3, parts 0..3, allocation generation 41, create attempt 1. Every plan carries the returned original backing ref/RID/backing serial and SHA-256 of its exact upload bytes.

| Part | Storage | Bytes | Upload role | Initial words |
| --- | --- | ---: | ---: | --- |
| 0 | RawResults | 24 | METHOD_RAW_RESULTS=15 | six `a5a5a5a5` words |
| 1 | FaultPlane | 16 | METHOD_FAULT_PLANE=9 | four zeros |
| 2 | ResultEpilogue | 4 | METHOD_RESULT_EPILOGUE=16 | zero |
| 3 | RawParameters | 16 | METHOD_RAW_PARAMETERS=14 | 100, 200, 0, 1 |

Capture and Body each use actual set0 bindings 0/1/2 for the first three children, and set1 binding0 for RawParameters. Both real GLSL stages reference all four resources, so set1 cannot disappear as unused. Capture writes an intermediate raw word 204, fault word 200, and epilogue 2. Body writes raw words 100..103, fault0=0, and epilogue0=1. RawResults words 0..3 are the logical payload; words 4 and 5 are the two trailing guards. Body leaves both trailing guard words untouched. Capture local size is 1; Body local size is 64, with a four-element envelope and one actual dispatch group.

The ordered readback partition is 24/16/4 bytes at result offsets 0/24/40, with exact actual source child generation and backing serial. The positive result must be exactly these eleven words (44 bytes):

```
00000064 00000065 00000066 00000067 a5a5a5a5 a5a5a5a5
00000000 00000000 00000000 00000000 00000001
```

## Original protocol exercised

`v2_probe.cpp` freezes RecordV2=448 (explicit capture/body set counts), ReportV2=328, RangeV1=48 and ObservationV1=56 through static assertions against `gdextension_interface_v2.pinned.h`. Header SHA-256 at this handoff is `971a1ab90541900c50cde6eb45f62e7f5fcc1a95cbcb9a8632fab792a9835289`.

The original shared QRT slot/counter handles kind11 prepare, kind8 bounded upload/segment reports, and kind10 prepared V2 record, as well as existing original storage/native-unit create/retire. No generic binding-upload operation or second mailbox is created. The fixture checks copied slot exact replay, preactivation abort, one activation and one delivery acknowledgement using the existing scaffold.

Positive mode checks:

- Four original plans, exact real upload segment ranges/backing serials and no possible effects.
- Copy and release are busy before recording/readback; the destination remains untouched.
- Exact original prepared/record replay, same request/unit digests and original ticket report identity.
- Three nonzero, pairwise distinct actual asynchronous readback request serials; descriptor/range identity and aggregate size exactly match the frozen record.
- Every returned original fact has known native issue and exact successful completion; readback facts exhaust each original source range.
- Exact copy44, all output values, both raw guards, fault plane, epilogue; completed result cancellation is refused.
- Ticket acknowledgement once, then free both sets per phase, native-unit physical free events/acknowledgement, all four original storage physical free events/acknowledgement, bridge/session/context closure and guard/code-hold teardown.

Cancellation mode uses a separate probe process with `TICKET_MODE=2`. It admits all four original uploads, then cancels the retained uploaded prefix before record. Further upload/record admission must be closed, no readback/result is published, the exact original upload facts physically settle, and the same actual child/unit/bridge cleanup is acknowledged. It does not claim to test cancellation during a partially recorded readback.

The host mode is ordinary macOS startup with `--rendering-driver metal --rendering-method mobile --render-thread safe`; no headless or doctest rendering stand-in. As in the inherited upload scaffold, this control uses an owner that can create the actual qualified uniform sets on the main RD execution thread. It does not certify an external ANGE Store owner running on a separate thread.

## Prepare and run

Rebuild only these small dylibs:

```sh
python3 work/native-multireadback-20261101/runtime/prepare.py
```

If main coordination intentionally updates the generated ABI header, repin explicitly and rebuild. Record/Report layout static assertions prevent an accidental size change:

```sh
python3 work/native-multireadback-20261101/runtime/prepare.py \
  --pin-header work/native-multireadback-20261101/candidate/core/extension/gdextension_interface.gen.h
```

Use the actual newly built executable and its controlled-build metadata with verified hashes; do not substitute an old engine. `run.py` requires both pins, validates the metadata's Executable selection and artifact SHA/size, records actual `otool -L` linkage, and refuses source/header/probe-DSO/container drift. For this monolithic executable, core is part of the executable; an unlinked `libgodot` DSO is never claimed as loaded host evidence. The default action generates both actual native Metal containers from GLSL, then runs positive and cancellation controls:

```sh
python3 work/native-multireadback-20261101/runtime/run.py \
  --binary /absolute/path/to/new/godot.macos.editor.arm64 \
  --binary-sha256 <verified-new-executable-sha256> \
  --controlled-build /absolute/path/to/new/godot.macos.editor.arm64.controlled_build.json \
  --controlled-build-sha256 <verified-controlled-build-json-sha256>
```

For separate central orchestration, use `--action generate`, then `--action run`; the second stage requires the same exact executable/controlled-build/linkage and source/container hashes. `--case positive` or `--case cancel_prefix` selects a single native control. Logs and JSON receipts go to `runtime/results/<timestamp>/`. `containers/GENERATED.json` pins the actual host identity/linkage and generated container contents. `--core/--core-sha256` are optional and accepted only when `otool` actually identifies/resolves that specific `libgodot` dependency; the monolithic host refuses them.

`BUILD.json` records the actual clang commands, source/header hashes and both dylib hashes. Both builds completed with return code 0 and no compiler warnings. `prepare.py` and `run.py` passed Python byte compilation; `_phs1_probe_init` is exported by the built probe. GPU execution, generated native containers, native success markers and clean teardown remain unverified until the central run.

The two trailing guards follow the original logical-plus-two-tail-guards contract; no leading guard or logical index shift is introduced. The native control still does not supply an ANGE request or owner acceptance.

## Applicable real V1 neighbors

`legacy/` contains fresh copies of the existing `qrt_ticket` and `qrt_upload` native scaffolds and their original one-buffer container generator. The original evidence files are unchanged. Both copies include this runtime's current pinned generated header and statically assert V1 Record312/Report184 and the current upload ROLE_COUNT=19. In particular, the upload negative uses the current enum sentinel rather than treating the old numeric value 8 as invalid after the Method roles were added.

Compile only those two copied DSOs:

```sh
python3 work/native-multireadback-20261101/runtime/legacy/prepare.py
```

Add `--include-legacy` to the pinned-host command above to generate/run these two neighbors after V2, or use `--legacy-only` to select only them. They share the same executable/controlled-build/linkage pin validation, clean-exit/guard-teardown checks and final JSON receipt. Their generated containers receive a separate exact-host/source SHA receipt in `legacy/ticket_containers/GENERATED.json`. `legacy/BUILD.json` records the rebuilt DSO/header/source identities. No old compiled DSO is used, and no broader native GPU suite is implied.

At handoff both legacy probes compiled with return code 0 and no compiler warnings. Their new containers and native runtime results remain pending the central engine build.

## Store destination allocator-issuer regression (mode 3)

`v2_store_destination_probe.cpp` is an independent copy of the positive native control, built with `TICKET_MODE=3`. The existing four normal control sources/DSOs and default case selection remain unchanged. The same four original E05 allocations and exact kernels are reused. Part0 is the actual Store destination; no extra RawResults resource is allocated. Its logical range role is METHOD_DESTINATION=2, upload plan role METHOD_RESOURCE=18, and its native allocator issuer remains E05=1 with the actual nonzero E05 backing ref. Only the logical role changes.

Before the valid prepare, the probe submits a negative plan carrying that same real E05 ref while falsely labeling `owner_kind=E24`; it requires `ERR_UNAUTHORIZED` and a zero output ticket. This distinguishes logical Store/E24 identity from physical allocator authority.

Build just this extra DSO:

```sh
python3 work/native-multireadback-20261101/runtime/prepare_store.py
```

Select it explicitly with the same pinned-host runner arguments and `--case store_destination`. Successful GREEN requires the full positive three-async-result/native-fact/copy44/two-tail-guards/physical-ack checks, plus the preceding wrong-issuer negative. It does not cancel. Default `--case both` still selects only the existing V2 positive/cancel controls; the two V1 controls remain selected via the existing legacy flags.

The main coordinator explicitly authorized this Store-only run against the old pinned executable SHA `9e24b2281bf0b02735fb21b3e433a6142fe945d2856fd41c6f5f7d9965ab4575`. It produced the precise intended behavioral RED: real Metal, the wrong-E24-issuer negative refused with zero ticket, valid E05 prepare/replay succeeded, all four original uploads succeeded, then `record_prepared_v2` returned `ERR_UNAUTHORIZED=4`, reason16 for the real E05 destination plan. Probe exit was 91 at that exact call, not an unrelated shader/startup failure.

Exact evidence is `results/20261101-110408/store_destination.log` and `STORE_DESTINATION_RED.json` beside it. The RED receipt verifies the unchanged pinned host, exact refusal location/role/issuer and all prerequisite successes. This intentionally failed process does not establish physical cleanup success or a GPU result. Main coordination owns the one-predicate production correction, rebuild and Store GREEN run.

## Empty Method domain control (mode 4)

`v2_empty_domain_probe.cpp`, `empty_containers/`, `prepare_empty.py` and `run_empty.py` are independent additions. They preserve the previously verified sources/DSOs, their default selection, and the JSON ABI. The original actual E05 Store destination is retained: range role METHOD_DESTINATION=2, plan role METHOD_RESOURCE=18, nonzero original E05 backing ref. This control sets domain sample_count=0, range_begin=range_end=0, Body groups_x=1, and both capture/body push envelopes to zero. Physical result buffers/ranges remain 24/16/4 bytes; an empty logical domain is not an empty native readback.

Both independent GLSL copies contain `if (p.envelope == 0u) { return; }` before any effects. Nonempty branches remain identical to the original GLSL. GREEN still requires an actual recorded/completed one-group Body fact and three distinct nonzero actual async-result serials, exact partitioned native readback/copy44, and all original physical acknowledgements. Expected bytes are six unchanged `a5a5a5a5` destination words (four logical plus two trailing guards), four zero fault words, and a zero epilogue. No synthetic empty result is accepted.

```sh
python3 work/native-multireadback-20261101/runtime/prepare_empty.py
python3 work/native-multireadback-20261101/runtime/run_empty.py \
  --binary /absolute/path/to/the/source-matched-executable \
  --binary-sha256 <verified-executable-sha256> \
  --controlled-build /absolute/path/to/the-executable.controlled_build.json \
  --controlled-build-sha256 <verified-metadata-sha256> \
  --case empty_domain
```

This independent runner defaults to generating its guarded native containers and running only `empty_domain`; it never broadens into legacy or other GPU cases. Host/source/container identity checks are the same as the main runner.

Main coordination explicitly authorized generating these guarded containers and running this case on the old Store-capable pinned executable SHA `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58`. Actual Metal container generation succeeded; valid E05 prepare/replay and four exact uploads succeeded. The precise RED occurred at **QRT kind10 reserve validation**, before record dispatch, with sample_count=0/range0..0/groups1: `ERR_INVALID_PARAMETER=31`, reason2. Probe exit91 was at `record_prepared_v2_empty_domain`. It was not a shader/startup or arbitrary runtime failure.

Evidence: `results/20261101-111915/empty_domain.log`, `containers.log`, and `EMPTY_DOMAIN_RED.json`. The RED receipt verifies all prerequisites and the exact refusal stage; it makes no Body/completion/physical-cleanup success claim. Main coordination owns V2 empty-domain validation/common-preflight correction, rebuild and GREEN. V1 Numeric's nonzero-domain restriction is unchanged by this fixture.

## Source-matched empty-domain GREEN — 2026-11-01

`../EMPTY_2D_PINNED_HOST.json` fixes executable SHA `0d1bf7c24e9222c7880ac09b4e47283523193c815d9b9069d3fec4f4e1035b83` and actual monolithic controlled-build identity. `scons platform=macos target=editor tests=yes -j4` returned zero; all 7,187 recorded source hashes remained unchanged through that build. The source window deliberately includes listed dirty physics/reload/particle changes rather than claiming a clean HEAD binary.

`results/20261101-113943/results.json` records actual Metal container generation and empty-domain runtime, both rc0, all checks true and host unchanged. The original zero domain executes an actual one-group Body, produces three distinct async readback serials, returns the expected unchanged Store/zero FaultPlane/zero Epilogue bytes (44 total, including two trailing guards), and reaches physical acknowledgement. This closes the precise old-host reserve-validation RED above without changing either SDK layout or the V1 nonempty rule. Qualification neighbors pass 185 cases / 3,141 assertions (`../empty-qualification-neighbors.log`, rc0); the two texture driver-callback rejection diagnostics are the existing explicit negative controls. The test source was subsequently formatted without semantic changes. This is native engine evidence, not an ANGE Method product or owner acceptance.
