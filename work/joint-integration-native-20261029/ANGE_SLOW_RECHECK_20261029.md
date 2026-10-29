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

## Isolated CLI fixture recheck

With `bin/libange.macos.template_debug.arm64.dylib` present, rerunning the single `a complete roster produces no census output*` case advanced past library discovery but failed with rc **42** (1 case, 4 of 12 assertions failed). The original extension initialization logged `[RML-ABI]` mismatch: expected `clang:21.0.0 (clang-2100.3.34.2)|libc++:220106`, observed `clang:22.1.5|libc++:220105`, with the same listed SDK digest. Its 120-second watchdog killed the non-completing server and reported `RESULT:ENV_CRASH`; the preceding ABI error is the observed initialization failure. This later recheck does not establish which library files were present during the aggregate run. No ABI check was bypassed.

Local `CLI_DSO_DISCOVERY_RECHECK.log` SHA-256: `e1ba01b9315aa8845e40c2dd83fb9b2066e9893d5d3a13bca61e2042441dd333`. Post-run inventory SHA-256: `bin/ange` `0dcf53d4cde9ec5dd3ca0c004868544d66b26e8b37bad75f367ac08538a99831`, `bin/ange_server` `5e438506c9ad9afe43e2b44dfbdd3e27443b8ea65870e9872301ea4684b64181`, ANGE debug DSO `599ad6e5cd5aa7abaad8187a65934dcb73e1caeb7a46667adc64601030b8b680`, RmlUi debug DSO `e21e1c4e1e5d05096118aa0132e6bbde9f902129d659f908245e920215e1fcf9`. These inventory hashes are not a claim of an immutable pair during the run.

## Isolated matching-library control

A later control copied the same CLI, server and ANGE debug DSO into `/tmp/ange-cli-abi-pair.GdLNjo/bin` and paired them with the generated RmlUi debug DSO from `extensions/rmlui/build/ange/.macos.template_debug.arm64/`. The selected two DSOs both contain the same exact SDK/compiler/standard-library ABI string. `ANGE_BINARY=<isolated bin>/ange ANGE_TEST_WATCHDOG_SECS=30 ANGE_LOADER_TEST_EXECUTABLE="$PWD/bin/ange_test_slow" ./bin/ange_test_slow 'a complete roster produces no census output*'` returned **0: 1 case, 12 assertions passed**. This isolates the observed runtime mismatch for that case; it does not prove the remaining aggregate failures or native GPU acceptance.

SHA-256: CLI `0dcf53d4cde9ec5dd3ca0c004868544d66b26e8b37bad75f367ac08538a99831`; server `5e438506c9ad9afe43e2b44dfbdd3e27443b8ea65870e9872301ea4684b64181`; ANGE DSO `599ad6e5cd5aa7abaad8187a65934dcb73e1caeb7a46667adc64601030b8b680`; paired RmlUi DSO `20dd10b872baf1c51fa75ae10e189eb3a827b8e4491296a784eee5df4e364b8c`; `CLI_MATCHED_PAIR_RUN.log` `14fed43eb448a310b962f15d45aa2e844399c869ac8b1f41f80b59c7b0f16137`. Engine assets and FX were linked from the repository, so this is a bounded runtime control, not a hermetic build receipt. The shared `bin/addons` runtime was not replaced.

## Matched-library census group

With the same isolated pair and linked engine assets, `ANGE_BINARY=<isolated bin>/ange ANGE_TEST_WATCHDOG_SECS=30 ANGE_LOADER_TEST_EXECUTABLE="$PWD/bin/ange_test_slow" ./bin/ange_test_slow '[test_runner][reset][census][guard]'` returned **0: 51 cases and 285 assertions passed**. CLI, server and both DSO SHA-256 values remained identical to the preceding matched-library control after the run. This verifies the selected census group only; it does not resolve particle inspection, schema, legacy golden, or qualification gates.

`CLI_MATCHED_CENSUS_RUN.log` SHA-256: `91a97b136aecfc095b4669cda1ac3ef610c26fa694b001f67fd872d6f32cd1ea`. The main SConstruct defines the generated RmlUi library and its `bin/addons/rmlui` installation as the `rmlui-extension` target; the isolated test did not modify that shared installation.

## Matched-library verb semantic group

With the isolated pair, `ANGE_BINARY=<isolated bin>/ange ANGE_LOADER_TEST_EXECUTABLE="$PWD/bin/ange_test_slow" ./bin/ange_test_slow 'verb_semantic: every probe runs in isolation'` returned **42: 1 case, 1,984 assertions, 1,970 passed and 14 failed**. All 14 failed assertions reported `GDEXTENSION_CLOSE_FAILED error=044 route=5` and exit 78 after their scenario's `RESULT:PASS`; the log contains no `[RML-ABI]` occurrence. The affected dynamic sections were attach#5, destroy#31, let_from#69, play_sound#86, play_vfx#88, shatter#95/#96/#97, slice#98, spawn#99/#100/#101/#102 and spawn_grid#103. Do not count the scenario PASS as successful process close, or infer a specific unscheduled holder from this log alone. The pair's server identity is as recorded above and is not asserted identical to earlier typed-close censuses.

`CLI_MATCHED_VERB_RUN.log` SHA-256: `456b167f92d2174bb588c59eb0f7583a44d0cc98b88e771b5c8ff8dafbaa2c94`. Remaining close and particle source failures continue independently of the census control.
