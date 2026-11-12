# Same-buffer COPY graph fix review

Reviewer `/root/review_vertex`; read-only production/test review. Only this work document is changed in this task. No build, test, commit, or shared-header edit. Frozen test TU remains unchanged. CPU graph/mock evidence cannot prove real GPU correctness.

## Finding and legal input

`RenderingDevice::_buffer_copy_recorded` (rendering_device.cpp:1124-1180) validates both source and destination extents and records actual driver IDs plus draw trackers. It does not reject equal RIDs. A 64-byte mutable buffer copied from [0,16) to [32,48) is a legitimate nonoverlapping same-buffer operation. The graph repair must preserve that input and exact native copy region, rather than add an equality rejection or copy through invented buffers.

`add_buffer_copy` (graph.cpp:2196-2212) unconditionally supplies `[destination_tracker, source_tracker]` and `[COPY_TO, COPY_FROM]` when source tracking exists. For one actual buffer those pointers are identical. `_add_command_to_graph` first overwrites its usage_index twice, then processes the tracker twice. COPY_TO first records the current command as writer. COPY_FROM next generates another barrier and overwrites usage_access with COPY_READ and usage with COPY_FROM, before reaching the explicit self-dependency error near line949. The caller still receives a recorded handle because this helper returns void. This produces both an error and partial graph state; checking only the final handle or dependency path misses the lost write access.

## Smallest contained repair

A repair can stay entirely within `servers/rendering/rendering_device_graph.cpp`, without a new ResourceUsage enum or shared-header signature. Normalize the proven same-buffer COPY pair once at `_add_command_to_graph` entry, before either tracker loop:

- Gate on exact command type TYPE_BUFFER_COPY, resource count2, equal nonnull tracker pointers, and the existing COPY_TO/COPY_FROM order. For defensive identity, the RecordedBufferCopyCommand source/destination IDs and tracker buffer_driver_id must identify the same nonzero buffer. These conditions cover this legitimate path without changing texture or arbitrary callback admission.
- Keep destination entry0 as the single tracking entry and reduce the local resource count to1. Keep its ResourceUsage COPY_TO so existing write classification, previous-reader consumption and writer publication remain intact. No arrays need to be mutated or allocated.
- Carry a local boolean for this normalized operation. At the existing `new_usage_access = _usage_to_access_bits(new_resource_usage)` assignment (near graph.cpp:705), OR in `_usage_to_access_bits(RESOURCE_USAGE_COPY_FROM)` before any resource transition, barrier, adjacency, or tracker access mutation. Using the conversion preserves the FORCE_FULL_ACCESS_BITS debug configuration. The effective access then contains both COPY_READ and COPY_WRITE, while usage remains the existing write-capable COPY_TO.

This is deliberately operation-specific normalization, not a global duplicate-tracker merge. An alternative dedicated COPY_READ_WRITE enum also works semantically, but touches the shared enum and classification/string/access/layout switches unnecessarily for this bounded fix. Merely setting resource_count1 discards the source read. Merely skipping the self edge leaves COPY_FROM/READ as the final state. OR-ing the tracker access only after normal processing loses incoming barrier destination READ. Replacing the use with STORAGE_BUFFER_READ_WRITE generates shader access bits instead of transfer bits.

With the contained repair, the existing loop continues to derive all ordering and barriers:

| Previous/next operation | Existing mechanism after normalization |
| --- | --- |
| Prior write to samecopy | write-after-write triggers barrier; source prior access, destination READ|WRITE; previous writer adjacency |
| Prior read to samecopy | COPY_TO is write usage; drains previous read list into dependencies; barrier destination READ|WRITE |
| First samecopy in frame | one tracker update, no self edge, writer=current, access READ|WRITE |
| Samecopy to next read | usage COPY_TO→COPY_FROM changes; barrier source READ|WRITE includes the write; reader depends on samecopy |
| Samecopy to next write/samecopy | write-after-write triggers barrier even though enum usage remains COPY_TO; subsequent writer depends on samecopy |

Usage comparison can remain COPY_TO because buffers have no image-layout mapping here; the stored exact access already expresses both effects. `_check_command_partial_coverage` and `_check_command_intersection` only refine consecutive attachment writes; buffers conservatively track the whole resource, which is safe for the disjoint ranges. Keep both trait modes (`driver_buffers_require_transitions` true/false) covered.

## Other commands with repeated or multiple trackers

- Compute/draw/raytracing list registration (graph.cpp:2314,2429,2681) already de-duplicates each exact tracker using its respective list index. In DEV builds, a different usage for the same tracker in one list is explicitly rejected. This repair must not silently merge shader usages or relax those contracts.
- Texture copy and resolve also pass destination/source pairs (graph.cpp:2853,2901). Texture parent/full/slice handling has explicit usage-equality and overlap checks near graph.cpp:720,795. A generic COPY_TO/COPY_FROM merge could legalize conflicting image layouts or swallow slice errors; scope the fix to TYPE_BUFFER_COPY and a real buffer tracker.
- Texture get-data has texture+buffer trackers; these represent different resource kinds and cannot be the same legitimate tracker. Buffer get-data carries source tracking only. Buffer update/clear track destination once.
- Texture update may repeat read-only source buffer trackers. BLAS/TLAS arrays may likewise repeat source trackers. Those are pre-existing duplicate-read handling concerns, not the proved same-buffer write/read case. They do not justify changing every duplicate tracker to write usage.
- `add_driver_callback` forwards caller-provided tracker/usage arrays directly (graph.cpp:2255). Its type must remain outside this special case; a generic coalescer could change arbitrary callback semantics without a defined compatible-usage policy.
- `_add_adjacent_command` and the self-dependency guards enforce graph invariants. Keep those guards; normalize this legitimate operation before reaching them.

## Regression scope and evidence status

Frozen `test_graph_same_buffer_copy.cpp` has standard Godot header and TEST_FORCE_LINK. Its three cases each exercise buffer transition trait false and true: (1) no engine error and both access bits retained; (2) W→R→samecopy→R→W dependency paths; (3) mock emitted per-command memory barriers and exact same-buffer copy offsets/size. The third case calls graph.end(false,false), preventing merged command groups or forced full barriers from masking missing bits. It asserts incoming WRITE→READ|WRITE, next READ observing prior COPY_WRITE, and final READ→WRITE. These tests are sufficient to falsify the observed graph error and several incomplete repairs; they are not backend GPU acceptance.

Root owns the unique build/test lane and plans an isolated RED run before production authorization. This review contains a proposed fix only; no production edits have been made, and no red/green result is claimed.

## Backend limit

Vulkan converts COPY_READ and COPY_WRITE independently to TRANSFER_READ/TRANSFER_WRITE (driver_vulkan.cpp:393-405). D3D12 enhanced access conversion also maps each independently (driver_d3d12.cpp:2084-2092). D3D12 legacy command_copy_buffer performs its own COPY_SOURCE/COPY_DEST transitions (4008-4021), with API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS true only for the legacy path (6032). The mock true/false trait tests verify graph bookkeeping for both policies; they cannot establish that every D3D12 mode supports an identical native buffer copy. Any backend-specific limitation is separate from this graph repair and does not authorize rejecting legitimate RD nonoverlapping same-buffer input.

Read time: 2026-11-07T01:37:07.784006+00:00

- `servers/rendering/rendering_device_graph.cpp`: `08fa9b8b4029f9011fb310759ab2182e9c449e08e5a2e9aa5072106fa851c70d`
- `servers/rendering/rendering_device_graph.h`: `ae8081a0aeffe0a4acc4b5646622eb6fbf298416353f63592c6a901bc6bcbbcd`
- `tests/servers/rendering/test_graph_same_buffer_copy.cpp`: `d29e8025ac51eca34014bf332924b0b4477309e6e47a65643c10c01e644f7b29`


## Final actual-source review and root runtime receipts

The actual active graph.cpp diff was independently read after root implemented the repair. It contains only the two intended insertions in `_add_command_to_graph`: exact TYPE_BUFFER_COPY/count2/equal nonnull tracker/TO-FROM gating and actual nonzero source==destination==tracker.buffer_driver_id identity; local count reduced to1 before both tracker loops; new access merged through `_usage_to_access_bits(COPY_FROM)` at the initial new_usage_access assignment. The original dependency/barrier/write/read-list logic and self-edge error checks remain unchanged. No generic texture/callback/shader-list merge, allocation, shared enum/header/API change or legal same-buffer rejection was introduced. This matches the contained repair described above; no blocking source issue found in this diff.

Root runtime artifacts were read, not rerun by this reviewer:

- work/native-output-continued-graph-red.json/log: exit1, 3 cases/94 assertions with14 failures. The log explicitly reports the self-dependency errors, lost tracker COPY_WRITE and missing outgoing write→read barrier. This confirms that the original regression is observable rather than an assumed RED.
- work/native-output-continued-graph-green.json/log: exit0, 3 cases/94 assertions all pass; complete green log has no ERROR diagnostic. Different binary SHA256 values are recorded in the JSON receipts.

Counterfactual source audit (no mutation/test runs by reviewer): skipping only the self edge still loses COPY_WRITE and fails cases1/2 plus outgoing-barrier case3; deduplicating to COPY_TO alone loses COPY_READ and fails cases1/2 plus incoming dstREAD case3; deduplicating then OR-ing only tracker access after processing leaves incoming barrier dstWRITE-only and fails case3. Skipping the self error and OR-ing after the duplicate second pass instead leaves COPY_FROM usage and no following same-FROM read barrier, which case3 also detects. No additional case was necessary to reject those incomplete repairs.

Disposition: ACCEPT for the proved CPU graph dependency/access/barrier bookkeeping regression. Source and receipts support that scope. Tests use mock driver callbacks and a headless/dummy host; neither this acceptance nor the transition-trait true/false cases establishes actual Vulkan/Metal/D3D12 GPU execution correctness or backend acceptance of same-buffer native copy. No new source/test edits, build, or Git operation was performed by this reviewer.

Final review time: 2026-11-07T01:51:57.264274+00:00

- `servers/rendering/rendering_device_graph.cpp`: `a6512f39ebbec0b0d36e4866d58e42e47c2e423bf17e51c27606945663475d64`
- `tests/servers/rendering/test_graph_same_buffer_copy.cpp`: `d29e8025ac51eca34014bf332924b0b4477309e6e47a65643c10c01e644f7b29`
- `work/native-output-continued-graph-red.json`: `4fd7cb4c3751d02413c241b89a01f5133b5ecfdbac9c5132420c627f470352b3`
- `work/native-output-continued-graph-red.log`: `2a51afc0fb74415f34123d5d198c76688cc838ebde8bd452c2cf9334ea674dbc`
- `work/native-output-continued-graph-green.json`: `1b341c5627803b51c1d58f9fc65b2a19719184ead810c2573411ce14dac4c286`
- `work/native-output-continued-graph-green.log`: `88e48667157632d6e16c4a1820eab6d0446cf4f8eb322f86bf2f118eaab02e42`
