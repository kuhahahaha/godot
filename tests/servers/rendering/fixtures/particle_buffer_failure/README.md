# Particle buffer rejection and recovery

Run `run.py` with the built macOS Editor, `--renderer mobile` or `forward_plus`, `--dimension 2d` or `3d`, and a log path. The runner requires a real Apple Metal device and checks that a 1,073,741,824 particle request is rejected by the RD buffer limit, a frame completes, and changing the same particle node to one particle produces red pixels. It rejects unexpected engine and script errors. The four combinations exercise Canvas, Forward Mobile, and Forward+ drawing after the rejected allocation.

This checks the oversized-size rejection path and visible recovery. It does not inject an actual device allocation failure after a valid particle buffer has been created.
