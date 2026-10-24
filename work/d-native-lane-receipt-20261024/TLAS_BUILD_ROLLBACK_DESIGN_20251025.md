# TLAS Build Rollback Design

Date: 2026-10-25

## Confirmed failure modes

`RenderingDevice::tlas_build()` currently mutates live state before all input
validation completes:

1. Reusing an existing instance buffer advances `used_size` before validating
   hit SBT ranges and BLAS identities.
2. `_tlas_remove_blas_dependencies()` clears the prior dependency set before
   the new instance list has been accepted.
3. The validation loop inserts new reciprocal TLAS/BLAS links incrementally;
   a later invalid instance leaves a partial dependency graph.
4. Instance bytes may already be written into the mapped buffer before a later
   validation failure.

These are source-confirmed and distinct from the previously audited TLAS
instance-buffer allocation and deferred-memory paths.

## Safe repair shape

Use a two-phase build transaction:

- Phase A: validate every instance and resolve all BLAS pointers, decoded hit
  SBT offsets, types, invalidation state, and draw trackers into temporary local
  vectors/sets. Do not touch `used_size`, dependency sets, or mapped bytes.
- Phase B: reserve/select the instance-buffer range, write all instance bytes,
  replace reciprocal dependency sets atomically, and submit the graph build.
- If phase B cannot reserve/create/map a buffer, leave the prior dependency set,
  `used_size`, and `invalidated` state unchanged.
- Set `invalidated = false` only after graph admission succeeds.

## Narrow verification plan

Add a focused test with one valid BLAS instance followed by an invalid instance
(hit SBT range, wrong BLAS type, and invalidated BLAS as separate cases). Assert
that each failed call preserves:

- the original instance-buffer `used_size`;
- the previous TLAS/BLAS reciprocal dependency sets;
- the previous `invalidated` state;
- the absence of a new graph build submission.

Then run the existing acceleration-structure source tests plus `git diff --check`.
A runtime/GPU receipt still requires a complete build with the raytracing test
fixture enabled; that evidence is unavailable because the current full build is
blocked by `No space left on device`.

## Ownership decision

No source mutation was committed. `servers/rendering/rendering_device.cpp` is a
mixed dirty migration file with unrelated owner-unattributed changes, so the
transactional rewrite cannot be safely isolated in this worktree.
