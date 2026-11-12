# Scene unregister bounded FIFO regression

Source implementation: `428d66ec62` (base `4a1ff33a5c`). Added `NativeScenePhaseCloseFifoFull`; no production or ABI change. A real full Core FIFO refuses the unregister wake while retaining both original request/control identities, QUEUED scene-waiting state and relay custody. The registration closes immediately, retries after sync return one original control per turn, and the holder closes with exactly one original free, without a draw.

Build: `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2` exited 0 in 15.71s.

Focused Scene phase/close FIFO/raster join: 4 cases, 5649 assertions, exit 0. Full ProductionRequest selection: 97 cases, 59065 assertions, exit 0. Logs: `BUILD.log`, `FOCUSED.log`, `PRODUCTION_REQUEST.log`. Broader run prints three expected render-pipeline factory failure diagnostics from existing negative cases; focused run prints none. Independent non-author review: `REVIEW.md`, spec PASS and quality PASS.

This is real Core FIFO and RD ownership/control behavior using a CPU test driver. It does not prove Metal output, Sprite LE192 visible draw, or whole-product closure. No pre-fix engine failure was expected: this adds coverage of an existing correct refusal path.

Built editor: 157342520 bytes, SHA256 `48629ef95d6f49e52fe9a7c998093da9ede3cace5b310c8f48f45bb24f72a2bc`.
