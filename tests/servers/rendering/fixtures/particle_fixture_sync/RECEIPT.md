# Particle source-extraction fixture synchronization

The 2026-11-06 current-source sweep ran 22 lightweight particle fixtures. Before synchronization, 19 passed and three failed to compile because their `Particles` test doubles lacked the production `external` member. The updated fixtures now all pass; production source is unchanged.

- `particle_aabb_flags`: 20/20 assertions in each float/double configuration. Added immutable external bounds, native readback bypass, and native data-size rejection assertions.
- `particle_deferred_retry`: 3/3 aggregate assertions. Added external queue removal without preparation/simulation/state mutation; native failures retain pending retry and recovery clears it.
- `particle_trail_binding`: 20/20 assertions. Added external setter state preservation, allocation bypass, and uniform rejection for both existing and absent resources.

Five temporary-source mutations delete the relevant production guard, one function at a time. Every mutant exits 1 with assertion failures, confirming the new checks detect guard removal. Temporary source copies are never written over production files. `RECEIPT.json` records the exact removed statements, extracted function, stdout/stderr, actual host, source hash, fixture hashes, and the original compiler failures.

Verification commands: `python3 tests/servers/rendering/fixtures/<fixture>/run.py` for each of the 22 entries in the JSON `after` list; `git diff --check`.

Scope: source-extracted C++ runs against sequential CPU resource mocks. `particle_external_guard` also validates generated SPIR-V offline. There was no engine build, native-host execution, GPU execution, or renderer qualification. Parent-owned `particle_collision_base_size`, all native/GPU host fixtures, and native runners (`particle_heightfield_normal`, `cpu_particles_depth_sort`) were excluded. Current extraction produces existing unused-variable warnings in deferred retry/process failure, recorded verbatim in JSON.
