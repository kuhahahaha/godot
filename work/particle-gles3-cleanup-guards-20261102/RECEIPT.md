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
