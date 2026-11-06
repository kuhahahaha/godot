# Typed generation Withdraw evidence — 2026-11-06

`particles_external_withdraw_typed` now authenticates the original generation root through its retained Registry pin, exact device/request/context, particle RID, generation, and owner kind. Slot pins, foreign requests/contexts, stale generations/RIDs, and fabricated scalar matches are rejected. The original `_external_withdraw_body()` performs the state transition. After the real OBSERVE retirement reaches `RELEASED`, a retry carrying the same exact request/device/registry/generation correlation is idempotently accepted; unrelated correlations remain rejected.

Build: `scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3` exit 0. Focused result: `bin/godot.macos.editor.arm64 --headless --test --test-case='*[GenerationRootComponent]*'` — 8 cases, 4,920 assertions, 0 failures. Combined owner/output and request regression: `--test-case='*[ExternalOutput]*,*[ProductionRequest]*'` — 56 cases, 36,706 assertions, 0 failures.

Negative controls temporarily replacing the typed Withdraw implementation with unconditional `ERR_UNAVAILABLE` failed the focused test and triggered the expected uncleared-owner abort; restoring the exact implementation and rebuilding returned the focused suite to green.

Limits: this proves original-root custody and terminal correlation only. It does not add a cancel entry point, typed lifecycle frame, Sprite/LE192 writer, native completion callback, physical GPU retirement proof, or no-next-draw close.
