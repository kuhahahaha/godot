# Native six-link capture harness — 2026-11-02

This work item adds `capture_run.py`, a small process-capture control for a future fixed Godot/ANGE production run. It validates an explicit scenario, absolute pinned input artifacts, nonempty version/renderer/device fields, and use of the pinned editor executable. It records the exact expanded command, working directory, input hashes before/after, raw stdout/stderr, log hashes, return code, timeout and drift status. A timed-out process group is killed together so child processes cannot retain the capture pipes.

The harness intentionally does not call a qualification ABI, synthesize an issuer, infer a ticket owner, or promote a fixture to production. Every captured run carries `production_acceptance: NOT_PASSED`; native release/close remains `REQUIRES_SAME_OWNER_EVIDENCE`, and forced timeout carries `NOT_ESTABLISHED_AFTER_FORCED_STOP`.

## Verification

- `python3 -m unittest discover -s work/particle-native-six-link-audit-20261102 -p 'test_capture_run.py'`: **17 tests, 17 passed**, rc 0.
- Tests cover successful capture, literal command arguments, missing and mutated pins, relative paths, invalid timeout, launch failure, nonzero exit, input drift, timeout process-group cleanup, existing output refusal, missing artifact, explicit scenario, CLI nonzero status, and raw log hash retention.
- Godot editor version: `4.7.2.stable.custom_build.c72d05493`.
- Godot source HEAD: `1fe430f0b9e6e2dd154aedde74a46f214c13bbeb` (dirty worktree).
- Current editor SHA-256: `3d42b32530f53ad3b7ce4953d5431033fe0376f8236e40f0575736f773fe680d`.
- Generated SDK header SHA-256: `d5ed173d160be54958031efb9c4e18a7e4d4806dd0d1e40fd33002cfe6d637ce`.
- Qualification ticket source SHA-256: `0b2cbca92056711cd9535a27fb0e1dc777591b4c3fd8533e2d31b4012c71cd91`.
- A integration source audit: HEAD `79232c6d43b15af748ed5680276ef5a455ae8585` (dirty), `host_qualification_service.cpp` SHA-256 `9c8c89dfbbba4d432f42fbe4ad5e05a210752c30f71f0dac5e975835901d539f`, `e09_native_recorder.cpp` SHA-256 `6d320df703441ccf3185ba14f732936657ab75da38ca83c1f97e970e3eb6b323`.

The 17-test harness result is control evidence only. A real production issuer, fixed SDK/DSO pair, same-ticket completion/readback bytes, cancellation/failure settlement, release, typed-child retirement, bridge close and process drain are still required by `R02_CORRELATION_ACCEPTANCE_TABLE.md`.
