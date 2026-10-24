# Renderer cleanup seam follow-up blocker

## Review result

The remaining renderer/native seams were rechecked after the resource-retirement
propagation package. `RenderDataRD` already fail-closes retired camera/material
packets and both Forward+ and Mobile consumers route through the typed admission
methods. `MeshStorage` external-surface cleanup and `RenderingDevice` deferred
resource disposal are currently represented by a large shared dirty migration
from the old P01 probe names to the test capture registry.

## Why no independent source commit is safe

The current branch HEAD still contains the old P01 include/registry lineage,
while the working tree contains broad uncommitted changes in
`rendering_device.cpp`, `mesh_storage.cpp`, renderer headers, and the test
module. Any additional cleanup edit would either duplicate already-present
uncommitted changes or introduce symbols that do not exist at HEAD. Applying a
new renderer cleanup change before that migration is adopted would therefore
produce an unbuildable partial lineage and could alter ownership of dirty work.

No new binary selector, external owner, or runtime evidence is available for
this seam. Repeating typed admission guards would not advance an acceptance row.
The next safe action is to integrate the existing registry migration, then apply
and independently review the saved `resource-retirement-error-propagation.patch`
from commit `d585d3ecba` at the real deferred-free call sites.

## Verification

Inspected `RenderDataRD` packet retirement and Forward+/Mobile admission call
sites, plus `MeshStorage` external-surface and multimesh buffer release paths.
No source was changed in this round. No build or full test matrix was run.
The document itself passes `git diff --check`.
