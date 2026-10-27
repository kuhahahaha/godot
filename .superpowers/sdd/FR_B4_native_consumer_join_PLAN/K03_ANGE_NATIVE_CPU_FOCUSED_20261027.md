# ANGE native CPU focused regression — 2026-10-27

Against ANGE source `b6c362304b…` and DSO `a60b3fa5…`, the focused Catch2 suites passed:

- `[particle_system_bundle]`: 6 cases / 81 assertions
- `E05 *`: 43 cases / 907 assertions
- `E06 *`: 7 cases / 250 assertions
- `C02 *`: 13 cases / 402 assertions

The coverage is CPU/in-process semantic and native guard behavior. It confirms bundle publication/refusal, allocation/borrow boundaries, capture admission, and no-effect fail-closed paths. It does not issue U08/E09 owner association, operation/completion/readback authority, Ready, Verified, or GPU qualification.
