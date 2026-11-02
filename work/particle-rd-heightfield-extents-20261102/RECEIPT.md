# RD heightfield extents cache invalidation receipt

Date: 2026-11-02

## Scope

RD particle heightfield collision storage now retires cached framebuffer and depth texture when the collision X or Z extents change. Y-only and equal extents retain the cache; non-heightfield collisions and invalid inputs retain existing state.

## RED to GREEN

- Baseline source fixture (`--source-ref HEAD`): RED, 6/11 checks; aspect changes retained stale cached RIDs and invalid input changed state in the unguarded setter.
- Current source fixture: GREEN, `RD heightfield extent cache: 11/11 checks passed`.
- Cache retirement uses `RD::free_rid()` in framebuffer then texture order and clears both members.

## Verification

- `scons tests=yes platform=macos target=editor -j4`: rc 0.
- `[Particles]*`: 29 cases / 154 assertions, rc 0.
- `[ParticlesProcessRetry]*`: 3 cases / 47 assertions, rc 0.
- GLES3 heightfield neighbor fixture: 8/8 checks passed.
- Editor SHA-256: `3d42b32530f53ad3b7ce4953d5431033fe0376f8236e40f0575736f773fe680d`.

This is a native RD storage invalidation fix. The fixture does not claim live RD GPU allocation-failure qualification or full production renderer coverage.
