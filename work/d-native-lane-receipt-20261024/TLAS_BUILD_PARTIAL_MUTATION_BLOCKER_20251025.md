# TLAS Build Partial Mutation Blocker

Date: 2026-10-25

`RenderingDevice::tlas_build()` reserves/updates an instance-buffer slot before
validating every submitted instance. During the later loop, an invalid hit SBT
range, missing BLAS, wrong BLAS type, or invalidated BLAS returns an error after
`used_size` has already advanced; newly inserted BLAS/TLAS dependency links may
also remain partially installed. A failed build can therefore consume instance
buffer capacity and leave dependency state mutated.

A safe fix would stage validation and dependency changes before committing slot
usage, or roll back all mutations on every failure path. It was not committed
because `servers/rendering/rendering_device.cpp` is a mixed dirty migration file
with unrelated owner-unattributed changes.

No safe owner-attributed source mutation was made. Qualification remains
source/static only and all runtime/native/GPU/PCK/platform/performance/
production flags remain false.
