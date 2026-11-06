# Legacy heightfield native clear regression

Source base: 5d56fc5de36df5c1c5a7becf6fcfae6ed6d50557 plus this fixture and separately tracked internal normal-relay changes. Production Mobile clear flag was restored before final build; its source has no final diff.

Implemented the tests-only main-RD fixture using two independent base colliders. Both D32 targets are seeded .75, the actual scene renderer captures only the second, and production textures are sampled into an owned SSBO with asynchronous main-RD readback. The normal render loop remains enabled; no local-only submit/sync is called.

Final actual Metal run on Apple M3 Pro passed all 8 Mobile/Forward+ cases: untouched sentinel .75; empty0 and geometry y=-.5/0/.5 yielded .25/.5/.75. Every process exited0 with no ERROR/WARNING; final marker HEIGHTFIELD_NATIVE_MATRIX_PASS. See native-green.log.

Effective negative: replace only Mobile heightfield DRAW_CLEAR_DEPTH with DRAW_DEFAULT_ALL, rebuild and run the same fixture. Empty Mobile measured second_raw=.75, sentinel=.75, pass=false and process1; runner1. See native-negative.log. Restored original flag and rebuilt before the final 8-case green.

Build: scons -Q tests=yes platform=macos target=editor module_particles_runtime_tests_enabled=yes -j3 (exit0). Python compile, ruff0.15.8 check/format and git diff --check passed. Independent review found disabled render-loop progress dependency; removed disabling call, then reran actual full GPU matrix. Review otherwise found no blocker.

This is a legacy raster numeric regression, not E34/HeightFrame acceptance. It does not implement per-instance immutable content, COW, retained source/read uses, checked-failure unwind or no-next-draw typed closure. Failure/timeout teardown is not a lifetime qualification.
