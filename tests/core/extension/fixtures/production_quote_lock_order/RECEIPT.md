# Main SceneReady production quote/accept lock order

The synchronous Engine main-thread SceneReady path now takes the original main RD mutex before Registry. This allows an RD owner to finish its Registry work before quote/accept proceeds. Full original installation authorization remains under Registry, and the selected device must equal the locked original RD. Other owner-thread paths keep the existing nonblocking helper. Registry unlocks before RD through RAII declaration order. No ABI change.

## Verification

Build: `scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3`.

On the exact original request.cpp preimage, the two new `[QuoteLockOrder]` cases fail with ERR_BUSY (44), returned_while_rd_held=true: 2 cases, 34 assertions, 6 target failures, exit 1. With the exact approved patch restored, both pass: 2 cases/44 assertions, exit 0. Full `[ProductionRequest]`: 64/50331; `[ProductionNativeObserve]`: 22/17617; CPU `[ParticlesReadback]`, `[ParticlesProcessRetry]`, `[ParticlesTrail]`: 13/75, all exit 0. Full ProductionRequest emits exactly three expected `!pipeline.driver_id` diagnostics from existing negative pipeline-creation cases. MoltenVK deployment-target warning is unchanged.

The worker holds the actual fixture RD mutex, waits up to one second for main return, then acquires Registry while still holding RD. Both workers are joined before assertions. The condition window is a bounded scheduling observation, not a formal mutex-wait instrumentation proof; the actual old-source RED establishes the observed failure interleaving. An external 45-second timeout bounds the focused process. Quote metadata, actual accept/close holds, foreign-thread rejection and invalid producer/CodeHold output preservation are checked. Thread startup uses the actual Thread ID API and is_started(), corrected after an initial test-only run exposed the wrong Error comparison; only the corrected RED/GREEN is retained here.

Independent review: review_collision_fixture reviewed the lock patch and tests; the startup API oversight was disclosed and re-reviewed against thread.h. Source proof is limited to the real synchronous Engine main SceneReady lifetime, not arbitrary Object borrowing. This CPU Enrollment fixture does not establish real Metal scene acceptance. A new fixed editor must be built after commit and delivered to the original A runner before that acceptance can be rerun. Existing fixed3ee bytes are preserved.

Log hashes and exact source identities are in RECEIPT.json. The global Godot/particle queue remains active.
