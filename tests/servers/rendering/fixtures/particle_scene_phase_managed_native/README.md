# Managed Scene phase native fixture

Run with a real renderer (headless uses the dummy device):

```sh
bin/godot.macos.editor.arm64 --rendering-driver metal --rendering-method mobile --path tests/servers/rendering/fixtures/particle_scene_phase_managed_native --script run.gd
```

The main-thread script calls the original `RenderingServer.force_draw` API while the render loop is enabled. Actual `_draw` dispatches the typed phase callback. After the original RECORD returns, the harness disables drawing and proves a waiting RECORD can return through unregister without another draw. Forcing draw unconditionally bypasses that cutoff and can consume the waiting control before the close test.

The harness tests original adopted Callable/owner enrollment, typed-only delivery, original live physical frame and expiry, receiver snapshots, no-draw waiting mailbox return, holder/free acknowledgement, generation root return and request/enrollment cleanup. It does not establish E09 operation/cursor/schedule binding, C05 Scene lifecycle, HeightFrame/COW, or P01 complete numeric qualification.

Verified 2026-11-07 on Apple M5, Metal 4 Forward Mobile: two serial runs returned exit 0 with all 47 checks true. Native C++ source was 4476ac93362; editor SHA256 261feb38d57a5421c441b40e19995aea5ba1a66e4239768c08e5e95eb208c432. The existing external-particles draw-pass diagnostic remains visible; these runs prove only the checks explicitly listed.
