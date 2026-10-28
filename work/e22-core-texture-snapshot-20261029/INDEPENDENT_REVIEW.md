# Independent review — E22 core texture snapshot exports

- The exports are registered through the existing resource snapshot bridge and
  operate only on the retained original `CurveTexture` / `GradientTexture1D`
  object path.
- Tuple and plan comparisons are fieldwise; no comparison depends on C/C++
  padding. Wrapper and child revisions are explicit saturating content fields,
  not instance IDs or hashes.
- A missing Curve/Gradient child is typed `INVALID_SOURCE`; the export never
  invents zero content. Width, format, source kind, child presence and output
  size are checked before any write.
- Snapshot validates exact source tuple and complete plan, output/scratch
  capacity and alignment, and disjoint metadata/output/scratch spans. Prefix
  count is retained only for a real source failure after actual writes.
- Pixel semantics match the existing engine implementations: CurveTexture uses
  `i / width` and its RGB/Red mode; GradientTexture1D uses `i / (width - 1)`
  and its HDR/LDR representation.
- Debug, Release and actual `use_ubsan=yes` focused/regression evidence is
  present in `COMMANDS.json` and the raw logs.

Disposition: no P1/P2 finding in this bounded core-export slice. It closes the
D/core proc export gap only; E20 owner admission, live production adapter
caller, native/GPU execution, Ready and G1 remain outside this receipt.
