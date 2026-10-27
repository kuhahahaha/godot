# Godot native queue census — 2026-10-27

The Godot native side still has uncommitted work in the mixed worktree; it is not an empty queue. Two independent slices are present:

- GDScript coroutine/active-reload lifecycle: pending-state cancellation, resume reservation, stack cleanup ownership, cache failure behavior, and lifecycle tests.
- RenderingDevice/RD external mesh and packet path: external surface admission, generation checks, camera-source tuple propagation, packet capability/retirement guards, and focused native tests.

The current source snapshot rebuilt successfully with the dev editor. Focused verification is green:

- `[Modules][GDScript]*`: 13 cases / 162 assertions
- `[B4]*`: 20 cases / 419 assertions
- `[ParticlesTrail]*`: 1 case / 5 assertions
- `[K03][P2]*`: 22 cases / 350 assertions

The source edits remain dirty and must receive their own integration review/commit. The ledger commit containing this census is documentation only; it does not stage or commit the mixed native source changes.

The focused native tests do not create U08/E09 owner association, operation, completion, readback, packet publication authority, physical disposal, Released, Ready, Verified, or GPU qualification. Those claims remain false and remain external-owner or acceptance inputs.
