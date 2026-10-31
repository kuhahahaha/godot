# Particle buffer rejection and recovery

Run `run.py` with the built macOS Editor, `--renderer mobile` or `forward_plus`, `--dimension 2d` or `3d`, and a log path. The runner requires a real Apple Metal device and checks that a 1,073,741,824 particle request is rejected by the RD buffer limit, a frame completes, and changing the same particle node to one particle produces red pixels. It rejects unexpected engine and script errors. The four combinations exercise Canvas, Forward Mobile, and Forward+ drawing after the rejected allocation.

This checks the oversized-size rejection path and visible recovery. It does not inject an actual device allocation failure after a valid particle buffer has been created.

`--dimension motion_retry` is a separate test-build-only control on Forward+: it renders a normal 3D particle, enables TAA to request motion-vector instance-buffer rebuilding, injects one instance-buffer rejection after the main buffer already exists, and requires the same node to become visible again. This is a controlled rejection before the RD allocation call, not a physical device OOM.

`--dimension late_retry` injects one test-build-only simulation failure after the internal `clear` flag has been reset. The fixture requires that the failed frame does not reach instance copying, then explicitly restarts the same particle node and checks visible recovery. The restart is part of the fixture; this test does not establish automatic recovery without a caller request.
