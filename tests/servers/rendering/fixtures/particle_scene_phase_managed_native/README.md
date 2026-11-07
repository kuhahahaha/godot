# Managed Scene phase native fixture

Run with a real renderer (headless uses the dummy device):

```sh
bin/godot.macos.editor.arm64 --rendering-driver metal --rendering-method mobile --path tests/servers/rendering/fixtures/particle_scene_phase_managed_native --script run.gd
```

The main-thread script calls the original `RenderingServer.force_draw` API while the render loop is enabled. Actual `_draw` dispatches the typed phase callback. After the original RECORD returns, the harness disables drawing and proves a waiting RECORD can return through unregister without another draw. Forcing draw unconditionally bypasses that cutoff and can consume the waiting control before the close test.

The harness tests original adopted Callable/owner enrollment, typed-only delivery, original live physical frame and expiry, receiver snapshots, no-draw waiting mailbox return, holder/free acknowledgement, generation root return and request/enrollment cleanup. It does not establish E09 operation/cursor/schedule binding, C05 Scene lifecycle, HeightFrame/COW, or P01 complete numeric qualification.

Verified 2026-11-07 on Apple M5, Metal 4 Forward Mobile: two serial runs returned exit 0 with all 47 checks true. Native C++ source was 4476ac93362; editor SHA256 261feb38d57a5421c441b40e19995aea5ba1a66e4239768c08e5e95eb208c432. The existing external-particles draw-pass diagnostic remains visible; these runs prove only the checks explicitly listed.

The managed receiver query regression adds eight checks (55 total): two same-base height sources inside the broad envelope must be selected independently by each receiver's committed query. A second source inside one query still makes that query ambiguous; it must not affect the other query. A query outside both source AABBs must report empty. The old implementation fails five of these checks because it uses only broad-envelope pairs.

The Sphere value-snapshot lifecycle regression adds seven checks (62 total). Real source pose/radius mutation must update a newly collected snapshot while the retained snapshot stays unchanged. Free and replacement must update membership and produce a distinct source key; retained values remain readable after receiver/scenario/base release. This checks CPU value snapshots through the actual managed renderer fixture; it does not establish SceneSample/borrow, forced RID-slot reuse, GPU sampling, or retirement authority.
