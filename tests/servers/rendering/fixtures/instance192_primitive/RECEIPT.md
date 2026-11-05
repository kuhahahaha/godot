Instance192 topology specialization uses the actual pipeline primitive. Native points and lines preserve ordinary facing and skip polygon culling; triangles, triangle strips and wireframe triangles retain triangle handling. No ABI fields change.

Root verified actual-template RED→GREEN, 92 SPIR-V commands, editor build exit0, pipeline key 1 case / 2 assertions and ExternalOutputGuard 6 / 521 with empty output. Configured full suite: 2070 cases / 517049 assertions, 0 failures, with retained stdout/stderr diagnostics. Initial missing physics environment and zero-case filter are preserved in DELIVERY.json and are not passing coverage.

Evidence: `work/b4-instance192-primitive-20261105/`. This does not qualify actual GPU topology/facing, driver pipeline creation or the complete P2/P4 owner/consumer path.
