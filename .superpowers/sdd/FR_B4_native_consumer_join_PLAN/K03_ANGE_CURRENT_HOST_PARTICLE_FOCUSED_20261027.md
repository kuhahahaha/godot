# ANGE current host/particle native focused regression — 2026-10-27

The current ANGE test binary passed eleven focused host/particle tag groups: **54 test cases and 1,772 assertions**, rc0. Coverage includes host attachment and owner-thread guards, lifecycle/quiesce boundaries, typed root metadata and child cleanup, capture admission/layout, storage boundaries, C02/E05 binding, and particle system bundle publication/rejection.

The typed-root isolated-child test requires `ANGE_LOADER_TEST_EXECUTABLE` to point at the same `bin/ange_test`; the initial unset-environment run failed only at that fixture precondition, and the rerun with the exact executable path passed all 13 typed-root cases.

This is CPU/in-process native coverage. It does not supply the original U08 producer, public E09 same-request association, external operation/completion/readback authority, packet publication, physical disposal, Ready, Verified, or GPU qualification.

Evidence: `K03_ANGE_CURRENT_HOST_PARTICLE_FOCUSED_20261027_REPORT.json` and `/tmp/k03-ange-current-host-particle-focused-20261027.log`.
