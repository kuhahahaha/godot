# Particle lifecycle boundary audit

Date: 2026-11-05

## Scope

Audited GPU particle trail lifetime and storage reset behavior across the scene setters and RD storage backend.

## Evidence

- `GPUParticles2D` and `GPUParticles3D` scene setters clamp accepted trail lifetime to the renderer maximum and reject non-finite/negative inputs.
- RD storage validates finite trail lifetime with a minimum threshold, caps the accepted value at 10 seconds, frees dependent particle data, resets phase/tick state, marks the simulation clear, and emits the particle dependency change.
- Existing source tests cover 2D and 3D clamp/rejection behavior; rendering tests cover the backend predicate and setter contracts.
- The checked-in editor binary does not contain the current `[GPUParticles]` test registrations (`--list-test-cases --test-case='[GPUParticles]'` returned zero), so no new runtime pass claim is made from that binary.

## Outcome

No production edit was made. The lifecycle contract is already implemented in other owner dirty files; this receipt records the source/test boundary and the limitation of the stale binary.
