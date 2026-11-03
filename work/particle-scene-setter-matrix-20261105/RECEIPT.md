# Particle scene setter matrix

Date: 2026-11-05

## Verified source coverage

`tests/scene/test_gpu_particles.cpp` contains paired 2D/3D checks for:

- amount allocation overflow on CPU particles;
- amount, explosiveness, randomness, lifetime randomness, and speed scale rejection;
- finite preprocess time handling;
- visibility bounds rejection;
- trail lifetime rejection and renderer maximum clamping;
- draw-pass and trail-section bounds;
- rejected process requests preserving emitting state.

The corresponding scene setters and renderer storage predicates are present in the current source. These files already carry other owner dirty changes, so this audit does not restage or overwrite them.

## Runtime limitation

The available editor binary reports zero registered `[GPUParticles]` cases, so this matrix is source/test evidence only until a fresh tests-enabled editor build is available. No runtime pass claim is made here.
