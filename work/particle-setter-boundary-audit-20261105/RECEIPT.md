# Particle setter boundary audit

Date: 2026-11-05

## Scope

Audited the native CPU/GPU particle scene setters and renderer storage setters for amount, lifetime, preprocess time, explosiveness/randomness/amount ratios, visibility bounds, and draw pass count.

## Current evidence

- `CPUParticles2D` and `CPUParticles3D` reject non-finite or non-positive lifetime, invalid preprocess values, ratios outside `[0, 1]`, and storage-size overflow in `amount`.
- `GPUParticles2D` and `GPUParticles3D` currently contain owner dirty changes covering finite visibility bounds and ratio range checks; these files were not modified in this audit.
- RD and GLES particle storage setters independently validate amount allocation bounds and amount ratios before changing storage state.
- Existing focused tests in `tests/scene/test_gpu_particles.cpp` cover valid, out-of-range, and non-finite ratios plus CPU amount overflow. Renderer fixtures cover storage setter bounds.

## Outcome

No additional independent source edit was made because the relevant scene and renderer files already contain other owner changes. Duplicating or restaging them would risk overwriting ownership. Remaining work is focused verification and receipt reconciliation after those owner changes land.
