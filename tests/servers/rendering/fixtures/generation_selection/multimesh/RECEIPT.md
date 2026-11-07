# Unequal original mesh generation selection

Two test-only cases acquire two original CPU mesh owners with unequal 1+2 surfaces and three genuinely distinct material generations. They check every retained RID, mutation/free refusal, bounded reverse material-then-mesh release, directory retirement and root return. The stale second material case checks the retained two-mesh/one-material prefix. Existing default callers still create two materials.

Incremental build: `scons -Q tests=yes platform=macos target=editor -j2`, exit 0, 113.02 seconds. Focused `*[GenerationSelectionMultiMesh]*`: 2 cases / 418 assertions. Neighbor `*[GenerationSelection]*,*[GenerationGuardRefusal]*`: 6 / 974. Combined filter and binary identity are in the attached JSON: 236 / 96257, exit 0. Combined diagnostics are the existing three deliberate pipeline failures, three foreign-camera refusals and one deliberate invalid UTF-8 diagnostic.

Binary SHA256: `83805cff96640a756cd6d7365554d5c3a73be566bb044ec3aaa3a682c437a241` (157226440 bytes). No production behavior or ABI changed. This establishes CPU bookkeeping/lifetime coverage only; real preparation, public Create, geometry capture, renderer output and GPU completion remain separate. No counterfactual mutation run is claimed.
