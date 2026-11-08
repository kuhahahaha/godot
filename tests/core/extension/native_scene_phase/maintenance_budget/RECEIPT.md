# Real maintenance residual credit before cleanup content return

This test extends the actual failed readback-map lifecycle with a cleanup-only real Ref<SceneReadContent> in the original action slot. It verifies the genuine same-owner active native maintenance cursor before injection and never seeds cursor or native completion state. Scene key binding remains false; content validate refuses. The existing success/map-failure cases retain their original behavior; the cleanup case skips their eight reversible refusal mutations. State precedes the fixture and outlives its fallback cleanup.

Every original queued RELEASE receives one pass and one receipt. The test requires at least one actual spent-credit maintenance slice, including a slice which finishes the original cursor without invoking content. Zero-cost tail completion may legitimately allow a callback in the same slice. Content then returns Busy followed by success/destruction, each charged one pass/receipt; original owner consumption invokes no callback. All eight actual alias allocations are freed and both original requests reclaimed. Two callbacks and the destructor acquire Registry mutex from distinct joined threads.

Focused restored run: 1 case / 959 assertions, exit0. Combined: 224 cases / 95199 assertions, exit0; diagnostics are the established three deliberate pipeline refusals, three foreign-camera refusals and one invalid UTF-8 diagnostic. Final binary 157176552 bytes, SHA256 e38cc75bdf813a0cb56f7306ce36073a5fac8e30bd9b9ada9adc8e5a0ac56b64.

Counterfactual: give content the full original budget again after actual maintenance instead of subtracting consumed passes/receipts. Exactly four assertions fail: actual passes 2 exceeds offered1, callback slice passes2, invalid owner result, and no spent-credit finishing slice. The other 938 assertions pass and cleanup completes with normal exit1. Production source is restored with no tracked production diff before final green runs.

Build: `scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`. Adjacent JSON files retain exact test commands and binary identities. Independent source review identified and resolved state lifetime and zero-cost-tail assertions; it confirmed the original fixture budget was already1/1, now explicitly constrained at RELEASE.

CPU/dummy regression coverage only. No original Scene permission, accepted read/once, full retained source join or hardware GPU qualification is claimed. The long goal remains active.
