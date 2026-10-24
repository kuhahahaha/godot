# Allocation Identity Owner Capacity Blocker

Date: 2026-10-25

At `modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h`
lines 218-239, `SkinnedMeshCaptureTestAllocationIdentity::acquire_owner()` calls
`acquire()` first, incrementing `Entry::references`, then searches the fixed
owner array. If all eight owner slots are occupied, the search fails but the
new reference remains and the function still returns the serial. This creates
an ownerless reference that can prevent `physical_dispose()` and leak logical
lifetime accounting.

Minimal fix shape:

1. Locate the target entry and a free owner slot before incrementing references;
2. return `0` when no owner slot exists;
3. only then increment `references` and install the owner.

A narrow regression should fill eight owners, assert the ninth
`acquire_owner()` returns `0`, and verify `physical_dispose()` remains blocked
until the eight logical owners are released. The helper header is currently an
untracked migration file, so no safe owner-attributed source mutation was made.

Qualification remains source/static only; runtime/native/GPU/PCK/platform/
performance/production flags remain false.
