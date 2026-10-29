# ANGE aggregate slow recheck

The `16ac6eee9` source closure was built with `scons test-slow -j2`; build rc **0**. Link output contains macOS deployment-version warnings. The subsequent `ANGE_LOADER_TEST_EXECUTABLE="$PWD/bin/ange_test_slow" ./bin/ange_test_slow` completed with rc **42**: **774 cases, 688 passed, 86 failed; 12,607,910 assertions, 12,607,477 passed, 433 failed**. This is a failed aggregate run, not qualification evidence.

Particle failures include original method source inspection reporting `gpu.native_source.owner`, caller schema/revision assertions expecting 1/1 while the current source produces 2/3, changed source-span count, and dimensions legacy golden/catalog expectations. The first owner error is consistent with test declarations omitting the retained C05 policy; a corrective test change is under verification. Do not infer that all failures share that cause. Many CLI-dependent failures report `no ANGE GDExtension library for this platform and architecture` during the run; the precise state of the library at those invocation times has not been established.

ANGE commit `74041eaf859270f0ffcff76f0daa4e1b039663e1` explicitly returns no projection for E07 lifecycle identity, preserving the fail-closed boundary. `scons -Q -j2 test-gpu-qualification-unit` and `bin/ange_gpu_qualification_unit_test` both returned 0; the focused build log has no `LifecycleIdentityCorrupt` unhandled-switch warning. The unit reports `STATIC_ONLY_NO_COMPLETE_NO_VERIFIED`; it does not prove hardware execution, E07 decoder/map, E05 transport, Ready or G1.

Local logs in `/Users/macbookair/Documents/Work/ANGE/work/particle-mainline-postcommit-20261030/`:

| File | SHA-256 |
| --- | --- |
| `TEST_SLOW_R5_BUILD.log` | `11d3d510bf704c397c4aea3a32653226c80c2b7c51ae92e15b28a019945e8d56` |
| `TEST_SLOW_R5_RUN.log` | `7d833748a5deb1ecd8998353d1b8eaab5787198981925b53f00ae124d74f3f61` |
| `GPU_FAULT_EXPLICIT_REJECT_BUILD.log` | `ec4b3465941fe8b9689d63a2f0145f1e0e7276003724908e6c6c8b1dca272c4b` |
| `GPU_FAULT_EXPLICIT_REJECT_RUN.log` | `693e20969ea3de47dd64209899c1adf0b05740ec73dd8cfedb0b6fe234142356` |

The pending owner test recheck is not included in these results. The unaccepted `cfada39.patch` remains a conflicting proposal, not a landed owner bridge.
