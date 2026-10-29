# Godot P01 test contract repair — 2026-10-30

## Reproduction

With the `skinned_mesh_capture_tests` module enabled, the pre-fix Debug editor reproduced two failing P01 cases:

- `[P01] Image rejected enqueue drains holders and forbids late callback`: duplicate rejection poisoned the collector, then the test incorrectly expected `ERR_UNAVAILABLE` and a normal drain.
- `[P01] Collector preserves exact identity copied bytes and late close completion`: duplicate enqueue poisoned the collector, then the test incorrectly continued through the healthy completion path.

The pre-fix run returned 2 failed cases / 6 failed assertions.

## Repair

The tests now isolate poisoned duplicate-operation checks from healthy completion and rejected-slot checks. They explicitly close collectors and retire callback holders before asserting `drained()`. Production collector poisoning semantics remain unchanged; P01 now matches the existing fail-closed contract.

## Validation

- Build: `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j1 target=editor dev_build=yes tests=yes module_skinned_mesh_capture_tests_enabled=yes`
- Build rc: 0
- P01: `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='*[P01]*' --exit --no-colors --no-intro`
- P01 rc: 0; 91 cases / 9612 assertions passed
- CallQueue regression: `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='*[CallQueue]*' --exit --no-colors --no-intro`
- CallQueue rc: 0; 6 cases / 92 assertions passed
- `git diff --check`: rc0
- Debug editor SHA256: cd07b546dc34220448cf8580405c6ef8745758bccbfd51146a50439613d0bd57

## Production naming audit

A repository source census excluding `tests/`, `work/`, and build outputs found no `p01_probe` or `probe` production filenames. P01 fixture names remain under `modules/skinned_mesh_capture_tests/` and its `tests/` subtree, which is test-only and enabled only with `tests=yes` plus the explicit module option. No fixture or negative runtime result is promoted to GPU Ready.

## External godot-cpp availability

The Godot checkout has no tracked godot-cpp submodule or source path; the only candidate is an independent detached checkout at `/Users/macbookair/Documents/Work/ANGE/godot-cpp` (HEAD `b12a6c6`). It is outside this repository and not a pinned build input, so no extension/SDK ABI validation is claimable from this package. The Godot-only Debug build and tests above are the available evidence.
