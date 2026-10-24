# D native lane round-2 blocker — 2026-10-24

## Scope reviewed

The remaining directly visible Godot seam is `RenderDataRD` packet identity and
native-retirement admission (`servers/rendering/renderer_rd/storage_rd/render_data_rd.h`).
The current implementation and focused tests are already present in the adopted
lineage (`7830ab0de2`, `6b321d7a0d`, `d07dffb7c3`). No new production change is
authorized by the available owner contract in this round.

## Why this remains blocked

1. The existing binary does not contain the E15/E16 selectors. Running a selector
   against that binary would produce zero matches and cannot establish runtime
   evidence.
2. The Godot archive/build volume has approximately 5.8 GiB free at review time;
   the recorded archive failed with `No space left on device` in
   `work/e25-scope-split-20261024/isobase-build.log`.
3. The seam is a consumer of the external presentation/retirement owner. No
   adopted owner receipt supplies the runtime callback, lease, or GPU completion
   contract needed to extend it without inventing ABI or caller behavior.

## Independent review

The review confirms that the current D lineage has typed camera tuple admission,
previous-camera lineage checks, material-generation admission, typed retirement
receipts, and fail-closed retirement paths. The focused source tests exercise the
known transitions. They remain source evidence only until a rebuilt binary and
the adopted external owner are available. Repeating source-only guards would be
duplicate work and would not move a canonical acceptance row to runtime-qualified.

## Verification

- `git diff --check` passes for this document.
- No full build or full matrix was repeated; the disk-space blocker is unchanged.
- This document does not claim acceptance, native, GPU, runtime, PCK, platform,
  performance, or production qualification.
