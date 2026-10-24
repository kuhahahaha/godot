# Native/build/runtime qualification gaps — 2026-10-24

This note supplements the 26-row native coverage matrix. Canonical acceptance remains `pending` for every row.

## Exact gaps

- **Native binary gap:** the optional skinned-mesh module and E15/E16 selectors are absent from the existing focused binary. Source fixtures cannot receive native runtime credit.
- **Build gap:** the full macOS editor/tests/dev build is blocked by `No space left on device` during archive creation.
- **Runtime gap:** no rebuilt module-enabled binary has executed the focused selectors in this lineage; prior `0 tests matched` results are selector absence, not passing runtime tests.
- **GPU gap:** no hardware-backed draw/readback or renderer retirement qualification is available.
- **PCK/platform gap:** no packed production artifact or platform matrix has been produced from the current source state.
- **Owner/issuer gap:** E25 consumer/lease ownership, P01 production receiver ownership, and K03 public holder/issuer contracts remain unadopted.
- **Integration gap:** E15/E16 renderer/resource retirement changes are intermixed with dirty migration hunks and cannot be isolated safely.

## Restart conditions

Qualification work can restart only after all of the following are true:

1. free disk space permits a clean module-enabled macOS editor/tests/dev build;
2. the intended source lineage is frozen and dirty renderer migration hunks are owner-attributed;
3. P01 receiver, E25 consumer/lease owner, and K03 holder/issuer contracts are explicitly adopted;
4. the rebuilt binary contains the optional module and focused selectors;
5. focused runtime tests pass, followed by GPU/readback, PCK, and platform evidence collection.

Until then, source/static evidence remains bounded and all qualification flags stay false.
