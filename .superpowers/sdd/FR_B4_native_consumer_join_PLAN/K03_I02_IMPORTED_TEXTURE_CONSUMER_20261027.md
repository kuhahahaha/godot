# K03 I02 imported-texture consumer — 2026-10-27

## Scope and result

This package records an independent CPU/material consumer slice from ANGE
source snapshot `b00560bd636fd8d223984f834befd3dd75031ab0`. The exact I02 case
and its imported-texture family pass. The covered seam resolves effective disk
PBR overrides and routed graph selection in the material/lint consumer.

It does not issue P01/E34 ownership, native capture, GPU execution, packet,
completion, readback, PCK, Ready, or Verified authority.

## Focused evidence

- exact `*I02 effective graph and disk overrides*`: rc0, 1 case, 30 assertions;
- `[imported_texture_output]` family: rc0, 19 cases, 7671 assertions.

The raw logs and exact source/binary manifest hashes are preserved by this
ledger package. The source package reports a dirty shared worktree and keeps
this result scoped to CPU validation.

## Boundary

The consumer path covers material-output source, lint texture-output rules,
input registries, and blueprint builder routing. No live native source capture
or P01/E34 owner bridge is invoked. Numeric IDs, material graph handles, and
CPU test outputs cannot be promoted to R01/K01 or GPU authority.

All K03 positive claims remain false: U08, E09, operation, completion,
readback authority/bytes, packet publication, physical disposal, Released,
Ready, Verified, and GPU qualification.
