# CPUParticles3D mesh resource sync receipt

Date: 2026-11-02

`CPUParticles3D::set_mesh()` now disconnects the previous mesh's `changed` signal and connects the replacement mesh with deferred configuration-warning refreshes. The source fixture is RED at `HEAD` (missing both lifecycle checks) and GREEN on the current source at 2/2; the native `[CPUParticles]*` selection passes 11 cases / 47 assertions.
