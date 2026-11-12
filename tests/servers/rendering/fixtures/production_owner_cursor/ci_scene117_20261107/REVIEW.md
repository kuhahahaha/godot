# CI Scene117 housekeeping independent review

Source-only review on 2026-11-07, worktree `/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/worktrees/b4-renderer-continuation-20261104`, HEAD `71079130d7fc0fad0d9d6cdc0d32027cf41ebbe2`. Reviewed current staged and unstaged diff against HEAD; parent subsequently staged the 11 housekeeping paths. No source changes, builds, GPU runs, pre-commit launch, staging or commits by reviewer. Parent owns the pre-commit executions; the final targeted run receipt and log were read after completion, as distinguished from independent source equivalence below.

## Findings

No blocking issue found in the housekeeping slice. Scope is append-only .gitignore exceptions, three C++ formatter deltas, four C++ copyright-comment headers, four Python formatter deltas, three executable-mode corrections and one README spelling correction. The separate `tests/core/extension/test_production_request.cpp` diff contains intentional new wrong-thread test behavior; it is not a formatting-only diff and is excluded from the equivalence conclusion below. A blanket claim that all 12 currently changed tracked files have no logic changes would be inaccurate.

## Ignore rules and evidence preservation

The working .gitignore starts with all HEAD bytes unchanged and appends comments plus 168 `!/exact/rooted/path` exceptions. Every exception is unique, has no glob metacharacters, names a tracked file, and the exact path set equals `work/ci-scene117-20261107/PRESERVED_EVIDENCE.json`.

Independently rehashed every file in that manifest: all 168 byte sizes and SHA-256 values match. `git ls-files -ci --exclude-standard` returns no paths after the change, so no currently tracked evidence remains classified as ignored. Representative arbitrary new logs under work/, tests/servers/rendering/fixtures/, and .superpowers/ remain ignored using `git check-ignore --no-index`; the exception list does not broadly expose generated files. No tracked evidence content is modified by the current diff, and the review deletes or cleans no historical work/.superpowers artifact.

## Python equivalence and modes

Reviewed the entire four-file diff. For each file, parsed HEAD and working source with Python ast, removed and compared the top-level import set separately, then compared all remaining AST with attributes excluded. All four comparisons pass. The raw AST differs for three files solely because pathlib's top-level standard-library import moved after other standard-library imports; the upload runner already has identical raw AST. The diff preserves command arrays, arguments, environment clearing/setting, numeric oracles, timeout/error handling, subprocess options, report parsing and PASS strings. No runtime invocation was performed.

Files:

- tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_legacy_native.py
- tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_legacy_neighbors.py
- tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_native.py
- tests/servers/rendering/fixtures/particle_scene_viewport_invariance_native/run_native.py

Git mode diff contains exactly three 100644 -> 100755 changes: legacy native, legacy neighbors and viewport invariance runners. Their existing Python shebangs now agree with the hook requirement. Heightfield run_native.py was already executable; no extra mode change was added.

## C++ equivalence

Reviewed full whitespace-insensitive diffs for particles_scene_pairs_test_fixture.cpp/.h and renderer_scene_cull.cpp. Independent comparison confirms unchanged include set and unchanged source token text after whitespace/comments and braces are removed. This comparison alone is not a semantic proof, so each added brace pair was also reviewed: they enclose the same single original return, increment or assignment statement, and preserve the original if/else association, including scene_pair_revision saturation branches in _instance_pair/_instance_unpair. No condition, side effect, return Error, call ordering or declaration changes were found. The fixture source and renderer source reorder includes into formatter ordering; the header adds only a blank line.

No compile or binary equivalence claim follows from this review; a source-only formatting slice does not itself demonstrate runtime behavior.

## Gate coverage and limits

All 12 currently modified tracked paths are members of `CHECKED_FILES.json` and the recorded `PRECOMMIT_COMMAND.json` targeted --files invocation. No changed housekeeping path is omitted from that list. The repository hooks cover executable/shebang agreement, C++ formatting, Python lint/format/type checks and repository file-format/header checks as configured. No hook configuration or CI skip/filter is changed by this diff. clang-tidy is explicitly manual-only in .pre-commit-config.yaml, so the normal pre-commit invocation cannot be described as running it.

The recorded command is targeted --files, not --all-files. Its success would verify these listed files under applicable hook filters, not establish a complete repository-wide static suite or native behavior. The first parent-owned run returned exit 1 and auto-added standard copyright comment headers to both heightfield and both scene-pair fixture C++ files, plus README acknowledgment -> acknowledgment. Independently checked the heightfield files are identical to HEAD after removing only their initial comment header; the scene-pair comment headers add no tokens, and the README contains exactly that one spelling replacement. Parent staged the three mode changes so the index-reading executable hook could observe them. Subsequently read PRECOMMIT_GREEN_EXIT.json (exit_code 0, seconds 3.39) and PRECOMMIT_GREEN.log: applicable hooks report Passed, with unrelated language hooks skipped. This is evidence from the parent-owned targeted run, not a reviewer rerun; no compile or tests were run by reviewer. Functional validation of the concurrent wrong-thread test change remains separate.

## Observed final changed-file SHA-256

- `.gitignore`: `8d30b35ca050da9e8e9262a518741378b444e6615fe5f8eba523d3b6ef7dba7f`
- `modules/particles_runtime_tests/particles_heightfield_clear_test_fixture.cpp`: `26ade621174711b345dc9cfa68f7ed2a420681fa29debf792d42e3d87baeb072`
- `modules/particles_runtime_tests/particles_heightfield_clear_test_fixture.h`: `3a36274383dacc0375a5ccb4e6b5fb1bf7ab08720eb1f29568062c582cde7460`
- `modules/particles_runtime_tests/particles_scene_pairs_test_fixture.cpp`: `bdd23d201baa2d5ed7fe46873e5ae195789306f35e1d11cfff53daf9031cad54`
- `modules/particles_runtime_tests/particles_scene_pairs_test_fixture.h`: `424d9d0c0ab5e890b6c587f0edbec9ab6b51be58997b5e3469e1ae543372ffd1`
- `servers/rendering/renderer_scene_cull.cpp`: `095b1e47c73cd031d4a96ca697eea8f2b0999864925ad8bd93413b9f4316db32`
- `tests/core/extension/test_production_request.cpp`: `6cf1a0cadf968fa02d5c0266560bef97a5a56b1cae3d36c46ad3c14d6a5673c4`
- `tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_legacy_native.py`: `f6280b8de039ad826da2e310b784da48decf6d23455dd7708edbba917a18aeb2`
- `tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_legacy_neighbors.py`: `f03b801d07a516352c327dbbfac4bba880bc9549f1cd05c28fc63e34a45368df`
- `tests/servers/rendering/fixtures/particle_heightfield_clear_native/run_native.py`: `f1cede97e918c7ba7f5d5727be9549c1dfef7aa46dc3a6b113d72711403fc987`
- `tests/servers/rendering/fixtures/particle_scene_phase_managed_native/README.md`: `3cf2cea9adde92581300043e0644548771bda467715284be0e82e59692a0449b`
- `tests/servers/rendering/fixtures/particle_scene_viewport_invariance_native/run_native.py`: `13dfd6384007471250dee84bf813a58b85b983f68bb4bc00664fda51794e593d`

Parent-owned static result receipt SHA-256: `5441c36d4c69774b19bda3bde16280a687747c8a6db867a317358966cb4b7266`; log SHA-256: `b07ac65579890fe95593e2e9f3b3657dfcd05eb88be58b510f8b74126cf42351`.
