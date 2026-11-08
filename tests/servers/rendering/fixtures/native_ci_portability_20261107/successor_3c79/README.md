# Successor CI fixes and local acceptance

The exact remote run is https://github.com/kuhahahaha/godot/actions/runs/42377076633 at `3c79c5d8e107da32c07f1ffa017302de50896380`. Its captured snapshot has 18/20 completed jobs, 10 successes and 8 failures; two jobs were still running. This receipt does not claim that run or these successor changes are remotely green.

Three narrow fixes address its observed failures:

- Rename the raster report's local `serials` array to `output_serials`, avoiding the Registry member shadow rejected by strict Clang, GCC, MinGW and MSVC. The exact local strict Clang preimage fails and final source passes; original commands and outputs are in `ci/owned-shadow/`.
- Initialize the checked D32 pipeline's framebuffer/vertex creation facts in release fixtures too. The checked bind consumes those facts in every build. Restoring the old initialization reproduces ten failed assertions in the preserved local D32 negative control. This is an editor fixture mutation, not a claimed local release-template build.
- Capture the 2D pump callback's finish count inside that callback, before its semaphore publication. The Main step epilogue may already service and join the stop before the Main-side assertion runs. Exact-once, thread, trailing-cleanup and duplicate-finish checks remain intact; production physics is unchanged.

The final combined local macOS editor build exits 0. Its 157641032-byte host has SHA256 `7e2c5796c813a48b7587fbec6356c0ae9f90f735abf621cf0c6ea8e17b317774`. Configured full CPU acceptance executes 2269 cases / 585450 assertions with zero failures and 3 skipped cases; the focused run executes 55 cases / 6841 assertions, including all four 2D gates. `local/` preserves exact commands/environment/results and an XML audit; complete raw CPU output is archived alongside the native-depth allocation acceptance. These runs include the new allocation tests and do not imply GPU qualification from CPU fixtures.

Remote successful sanitizer lanes prove their compilation and unit-test exits only. Their logs include pre-existing runtime diagnostics; no globally diagnostic-free sanitizer claim is made. No local MSVC/UCRT build is claimed.

`FILES.json` maps each preserved original source/output to its archived bytes and SHA256. Gzip decompression was compared byte-for-byte with every original; command paths and original logs remain verbatim.
