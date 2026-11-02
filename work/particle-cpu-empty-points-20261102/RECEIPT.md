# CPU particle empty point emission

Date: 2026-11-02

2D and 3D point/direct-point emission now deactivate and skip a particle when emission_points is empty. Previously the switch break left the particle active at its default origin.

## Verification

- Source regression fixture against HEAD: rc 1, both dimensions report missing deactivation/skip.
- Current source fixture: rc 0, 2/2 checks.
- scons tests=yes platform=macos target=editor -j4: completed successfully; existing MoltenVK macOS deployment target linker warning.
- [CPUParticles]*: 12 cases / 49 assertions passed.
- [Particles]*: 29 cases / 154 assertions passed.
- Editor SHA-256: `a755924504cfd213ec278ce245374293f2fe58efef5635e8779ded836734d61c`.

The fixture verifies control flow in the emission branch. The native suites cover neighboring particle behavior; this receipt does not claim a live visual empty-emission observation.
