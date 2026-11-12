# Original Metal depth allocation quote acceptance

This tests-only fixture creates original D32 textures and their original framebuffer children on the real main RD/render thread. It observes native allocation/disposal facts. It does not record a particle producer, claim GPU content completion, release an output generation, or refund an E05 reservation.

The module requires `tests=yes module_particles_runtime_tests_enabled=yes` with 3D enabled. `SCsub` collects the new C++ source, and `register_types.cpp` registers `NativeDepthAllocationTestFixture`. The private RD friend is confined to `TESTS_ENABLED` plus this module. Root owns builds and GPU execution; neither has been run for this fixture at authoring time.

Run with a real Metal host and its explicit hash:

```sh
python3 tests/servers/rendering/fixtures/native_depth_allocation/run_native.py \
  --godot /absolute/path/to/godot \
  --sha256 ACTUAL_LOWERCASE_SHA256 \
  --out /absolute/new/evidence/directory
```

The runner starts two separate Mobile/Metal processes. Case 0 requests 64×64 and requires the actual quote policy to be `MetalHeapPool`; case 1 requests 8192×4096 and requires `MetalHeapDedicated`. The original allocator selects dedicated heaps from its native `heapTextureSizeAndAlign` requirement exceeding half its 128 MiB private pool block. The fixture never derives native bytes from the D32 payload extent. The large case may allocate approximately 128 MiB of native depth backing, plus host resources; the exact observed requirement/reservation/actual are captured in JSON.

One RT callback owns each entire interval while the render loop is disabled:

1. Reject zero width/height and the original maximum 2D texture size plus one, independently in both axes. Require `ERR_INVALID_PARAMETER`, an unissued/empty quote, unauthorized quote observation, and unchanged original backing/native-disposal serials, texture bytes and RD/Engine frame counters. Then issue an original quote and observe its descriptor, policy, positive requirement, owner reservation and alignment. Require two native children, one composite part and one shared attachment reference. Unexposed framebuffer/opaque costs stay typed unknown.
2. Feed that quote to the original quoted depth factory. Require `CreatedVerified` and positive actual driver backing within the quoted reservation. Create the original framebuffer and capture a separate pair projection; the framebuffer attachment must identify the same depth witness without adding a second depth allocation.
3. Record an original depth-clear draw list, close it, and use original native-disposal maintenance to materialize its real render pass and driver framebuffer. Require non-null original driver IDs in the live RDG cache and retain both children live. This avoids testing only the return of an empty lazy framebuffer cache. Then retire only the framebuffer. Its logical RID disappears while depth remains live. Bounded original native-disposal maintenance must reach framebuffer `RETURNED` while depth remains `LIVE` and aggregate returned remains false.
4. Retire depth. Its logical RID disappears while the framebuffer remains returned. Continue original maintenance until both actual driver-free witnesses are `RETURNED`; only then may the aggregate be true. Every observation checks that conjunction.
5. Require stable original RID/serial identities and actual backing across invalidation/return, idempotent repeated retirement, and unchanged `Engine.frames_drawn`.

The five snapshots are `live`, `framebuffer_enqueued`, `framebuffer_returned`, `depth_enqueued`, `returned`. The Python verifier independently checks their exact state sequence, identities, byte knowledge, shared attachment, quote bounds and all 29 check names. It saves both process logs, reports, source hashes and binary identity; source hashes do not establish build provenance. Any warning, error, crash, leak, timeout, missing fixture, unsupported path or process failure fails acceptance. Exit 77 identifies typed unavailability and never counts as GPU success. No headless/local-RD/CPU fallback is accepted.

Time limits only fail. Neither elapsed time, logical free, CPU sync nor a draw advances a witness in the fixture. Failed actual verification keeps the original witness and projection in the fixture, reports the original create error/outcome and `retained_on_failure`, and never converts the failure into success. The fixture intentionally avoids guessing cleanup on a failed/unknown prefix; the original RD still owns resource retirement.

Suggested targeted runtime RED: change the production pair aggregate from an AND to an OR of child `RETURNED` states. The framebuffer-only drain must fail `only_framebuffer_driver_returned`; an arbitrary crash is not a valid RED. Restore the implementation and rerun both cases. Driver over-reservation/unknown-actual failure injection belongs in the original issuer/factory tests; this GPU fixture supplies actual device observations without fabricating a quote or witness.
