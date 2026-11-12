# Native CI portability and FrameWait repair evidence — 2026-11-07

Evidence package for two production portability fixes and one FrameWait fixture repair. Source identity is FINAL_SOURCE_IDENTITY.json; this packaging task made no source edits, build, test/GPU run, staging or commit. All copied original command/output files preserve their bytes, directly or after gzip decompression. Remote excerpts are exact byte slices, including original ANSI escapes, with offsets and complete-log SHA256 references in REMOTE_FIRST_ERRORS.json. Full raw logs and XML remain at their repository-relative locations listed in RAW_HASH_REFERENCES.json; the large originals are not duplicated here. Log payloads use .log.txt (or .log.txt.gz) so ordinary *.log ignores do not drop them. Thirteen payloads that formatting hooks would rewrite are gzip archives with mtime0; ARCHIVED_ORIGINALS.json records both archive and original uncompressed byte counts/SHA256. COPIED_EVIDENCE.json and REMOTE_FIRST_ERRORS.json point to the actual archive paths while keeping source references unchanged. Decompress a .gz file with gzip -dc to inspect its original bytes; no text normalization was performed.

## Actual CI RED and bounded repair

Original commit: 1e0a60953996732d8e8e48278f4e5d974561f484, GitHub run42371357955 (kuhahahaha/godot). The eight job URLs, raw byte identities and first-error locations are in REMOTE_FIRST_ERRORS.json.

| Job | Original failure |
| --- | --- |
| 128400182167 Windows clang-cl | deprecated getenv/fopen in native_drain_diagnostic.h, warnings as errors |
| 128400182150 Windows MSVC | C2220/C4996 for the same secure CRT migration |
| 128400182171 Windows MinGW | Registry phase parameter shadows member |
| 128400182344 Linux minimal | same phase shadow |
| 128400182231 Linux Mono debug | same phase shadow |
| 128400182310 Linux Mono release | same phase shadow |
| 128400182259 Linux TSan | unit runtime LocalVector index0/count0 fatal, stack to reused native-slot FrameWait |
| 128400182293 Linux ASan/UBSan | same FrameWait test fatal; process exit134 |

The two production changes rename only scene_phase_call_relay's phase parameter to p_phase, and use MSVC _dupenv_s/free plus fopen_s while retaining POSIX getenv/fopen semantics. No warning suppression or CI-rule relaxation is added. The diagnostic path's existing absolute-path restriction remains. Exact historical patch/preimage hashes are referenced rather than nesting patches here.

The FrameWait test now opens the second epoch with actual RD::_begin_frame rather than faking graph.begin/primary flags. This resets the issue cursor; actual begin also disposes attach()'s synthetic BufferID61, accepted only by a test-local driver override. Original different-graph/same-buffer/same-slot, serial23/24, callback/completed-size/result checks remain. This fixes the fixture lifecycle and does not change production frame submission.

Original sanitizer reports include earlier unrelated runtime diagnostics. TSan signal-unsafe reports follow the bounds fatal signal handler. Preserving the first fatal and raw-log hashes does not certify other sanitizer diagnostics fixed. **No remote sanitizer successor GREEN is claimed.** A local macOS CPU pass is not a Linux TSan/ASan/UBSan rerun.

Historical reviews are retained verbatim under reviews/. They describe their then-current state. In particular, CI_AUTHOR_REVIEW's Mono log retrieval failure is historical: later successful RAW logs for Mono debug/release and MinGW are now in this package's hash references and excerpts. Historical author build/runtime pending statements are superseded only by the exact final local evidence below.

## Strict local syntax evidence

syntax/ contains original command JSON, actual return codes and byte-preserved output:

- INTERFACE_RED exit1 then INTERFACE_GREEN exit0: identical actual full interface TU command with strict shadow warnings enabled, before/after the parameter rename.
- DIAGNOSTIC_POSIX_GREEN exit0: actual modified header on native POSIX declarations.
- DIAGNOSTIC_WINDOWS_BRANCH_GREEN exit0: actual MSVC conditional branch checked with MinGW Windows CRT declarations and declaration-only OS/Thread stubs. This is not an MSVC/UCRT engine build; no local MSVC RED/GREEN is claimed. Prefix/stub source bytes are archived under syntax/ as .h.gz; original command paths remain verbatim.
- FRAME_WAIT_GREEN exit0: actual test TU syntax check; remote sanitizer fatal is its RED evidence, not an asserted local runtime reproduction.

Author header syntax checks used SHA473b204c6a4a3e3d5e314682e6d6114ad4763740b7554742252ebeac17287b75. Final header SHA8b7868f86f044e3d02258bead2eaaa8a22585788b53c1f5df6ce734f28af8c32 differs by copyright alignment and conversion from include guard to pragma once, reconstructed from exact author patch/preimage (reviews/HEADER_POST_HOOK_DIFFERENCE.txt.gz). Final host build covers that actual final header. Interface and FrameWait final hashes match their author-ready identities.

## Final local build and CPU evidence

Parent restored-source build command/result and complete small output are preserved in final/RESTORED_FINAL_BUILD*: scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2, exit0 in13.43s. Host157590776 bytes, SHA256 **5c806589c961a3a360790c29cf138164c801d66f1a385f5ce2b8dd9b60140852**. Packaging independently rehashed those host bytes (final/HOST_BYTE_VERIFICATION.json); this did not run it.

Original exact argv, environment, timings and process exits are in the three final/*_RESULT.json files. XML_RESULT_AUDIT.json independently parses the complete XML referenced by byte count/hash and records every FrameWait case plus failed expressions.

| Run | Exit | Executed cases | Assertions | Failures | Skipped cases |
| --- | --- | --- | --- | --- | --- |
| First full CPU, without explicit physics environment | 1 | 2263 (2262 passed) | 584848 (584847 passed) | 1 case / 1 assertion | 3 |
| Configured full CPU | 0 | 2263 passed | 584864 passed | 0 | 3 |
| Focused FrameWait + camera/capture/stage + Clustered selection | 0 | 27 passed | 841 passed | 0 | 2239 |

The first full run's sole failure was [SceneTree][PhysicsQueryRetirement] real backend dispatch control, tests/servers/physics_3d/test_query_retirement.cpp:80, CHECK(valid_config) expanding false. It is preserved rather than overwritten. The configured rerun explicitly set GODOT_TEST_PHYSICS_3D_BACKEND=GodotPhysics3D and GODOT_TEST_PHYSICS_3D_THREADED=0; it then passed the whole configured suite. No code waiver or skipped assertion fixed that configuration failure.

Both configured full and focused XML contain18 FrameWait cases/332 passed assertions, including reused native slot cannot overwrite an earlier graph issue with19 passed assertions. Focused27/841 is the aggregate of all listed categories, not a FrameWait-only count. The three full-suite skipped cases remain explicit; positive GPU execution is not inferred from CPU test discovery or UNRUN messages.

This package establishes historical remote failures, exact narrow changes, local strict syntax checks and parent final local CPU results. Remote Windows/Mono/MinGW successor builds and Linux sanitizer lanes still require their own results. No new GPU conclusion or external output permission is made here.
