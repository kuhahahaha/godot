# GLES3 particle cleanup null guards

Date: 2026-11-02

## Scope

GLES3 `ParticlesStorage::particles_free` and `particles_collision_free` now reject a null owner lookup before dereferencing particle state. This aligns the Compatibility renderer's direct native storage boundary with the existing RD guards and protects invalid or uninitialized RID calls. It does not reclaim uninitialized reservations and does not claim a normal RenderingServer queue path produces this input.

## RED to GREEN

- HEAD source fixture: RED, both free entry points lacked `ERR_FAIL_NULL` after `get_or_null`.
- Current source fixture: GREEN, `GLES3 particle cleanup guards: 2/2 checks passed`.

## Verification

- Focused source fixture was run before the edit and failed with both expected missing-guard diagnostics (rc 1), then after the edit passed 2/2 (rc 0).
- Scoped `git diff --check` passed.
- Full SCons/editor and live Compatibility invalid-RID runtime tests were not run in this turn; no GPU qualification claim is made.

## Post-commit verification — 2026-11-02

- Incremental macOS editor build: `scons tests=yes platform=macos target=editor -j4`, rc 0; elapsed 21.63s. The resulting `bin/godot.macos.editor.arm64` SHA-256 is `19f8c9f542fb64db713a03a31413c8df9622100f50d81969254195f9527760e4`.
- Focused native particle neighbor selection on that binary: 74 cases, 409 assertions, 0 failures (1955 skipped), rc 0.
- `python3 work/particle-gles3-cleanup-guards-20261102/run.py`: 2/2 checks passed, rc 0.
- `python3 -m unittest work/particle-native-six-link-audit-20261102/test_capture_run.py`: 17/17 tests passed, rc 0.
- `git diff --check -- drivers/gles3/storage/particles_storage.cpp`: rc 0.

These checks validate the bounded GLES3 source guard and nearby particle contracts only. They do not establish a production caller, fixed SDK/DSO association, or the six-link GPU lifecycle.
