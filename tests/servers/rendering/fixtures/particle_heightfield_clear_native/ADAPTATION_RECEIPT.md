# Transformed capture and direct legacy adaptation

Sources: e55f821a62787609c730350635ec03533362187f and 65c2e7237a18d359dec4dfad6a17aeb7abd927b3, parent e55f. Applied exact fixture changes onto the existing local runner; retained its explicit environment and disabled render-loop setting. No later source-branch changes were adopted.

Actual local centralized macOS arm64 editor build: tests=yes, module_particles_runtime_tests_enabled=yes, target=editor, debug_symbols=no; exit0. Built artifact SHA256 79dfbd35beebec50cd6559aa5ae6c9a4bd4195fcd1516bc05580a9f2deb4ee92 (pre-commit source build, version stamp60c5098c4).

Original run_native.py --display-driver macos --rendering-driver metal: exit0, 16 actual Apple M3 Pro cases, Mobile/Forward+8 each. run_legacy_native.py --display-driver macos: exit0,8 actual Metal cases,4 each. Both logs contain the exact success count, independent numeric oracles and no ERROR:/WARNING:. Host, metadata, fixture sources and runners matched their before/after hashes.

Capture log13545B SHA256 b840ce98021482e71c84e94ec870b40e6cb0141b88ee5a997fa7547db5f74435; legacy log7829B SHA256 716ea5f8f0013b1dfe7283d7b890606e35ed745fa9292c3b70a87b6882337995. Local manifests/results/logs: work/k06-owner-interface-20261107/SCENE_R5_{INPUTS_BEFORE,INPUTS_AFTER,RESULT}.json, SCENE_CAPTURE_R5.log, SCENE_LEGACY_R5.log.

Scope: real production D32 capture and planar direct ordinary legacy ParticleData. Does not prove automatic SceneCull collision-cache collection, slopes/neighbors, HeightFrame/COW, C05/E09/P01, or incoming-readback full closure. Source94's packaged16-case regression is an identical historical e55f log; it is not used as evidence of this local run.
