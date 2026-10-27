# Godot native queue landed package — 2026-10-27

The previously dirty native queue is now landed in three scoped commits:

- `2ca1cac188`: GDScript pending-state cancellation, guarded resume reservation, stack cleanup ownership, and reload/cache lifecycle behavior.
- `0db15cbe67`: RD external surface admission, generation checks, camera/material packet admission, renderer fail-closed guards, and focused native tests.
- `c2ebe25780`: Curve bake-cache invalidation on limit deserialization and the snapshot regression.

Only the listed native files were staged. Existing unrelated `work/`, `--out/`, device-exit, and other dirty artifacts remain untouched.

Post-commit verification is green:

- `[Modules][GDScript]*`: 13 cases / 162 assertions
- `[B4]*`: 20 cases / 419 assertions
- `[ParticlesTrail]*`: 1 case / 5 assertions
- `[K03][P2]*`: 22 cases / 350 assertions
- `[E22TextureSnapshot]*`: 3 cases / 1005 assertions
- dev editor rebuild: rc0, binary SHA-256 `d4938be792ae05f6868f73d13270c8ebe92292043eadadb6114717752924bb9f`

This package proves native source integration and focused local behavior only. It does not create U08/E09 producer association, external operation/completion/readback authority, packet publication authority, physical disposal, Released, Ready, Verified, or GPU qualification.
