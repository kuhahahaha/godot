# Particle recheck index

Date: 2026-11-06

This index enumerates the 2026-11-06 particle receipts currently present in the Godot worktree. It is a navigation artifact; it does not add runtime or GPU claims.

Indexed receipts: **13**.

## `particle-aabb-process-fixture-recheck-20261106`

- `particle_aabb_flags/run.py`: **17/17** float and **17/17** double checks passed.
- `particle_aabb_readback_guard/run.py`: **2/2** checks passed.
- `particle_gles3_aabb_readback_guard/run.py`: **7/7** checks passed.
- `particle_process_failure/run.py`: **9/9** dispatch cases and **2/2** inactivity-retry cases passed.
- `particle_process_progress/run.py`: **51/51** checks passed.

## `particle-boundary-fixture-recheck-20261106`

- `python3 tests/servers/rendering/fixtures/particle_aabb_readback_guard/run.py` passed **2/2** checks.
- `python3 tests/servers/rendering/fixtures/particle_gles3_aabb_readback_guard/run.py` passed **7/7** checks.
- `python3 tests/servers/rendering/fixtures/particle_process_failure/run.py` passed **9/9** process-dispatch cases and **2/2** inactivity-retry cases.
- `git diff --check` passed for the particle scene files inspected in this worktree.

## `particle-compatibility-fixture-recheck-20261106`

- `particle_gles3_unsupported_field/run.py`: Compatibility collision admission passed; unsupported 3D fields are skipped while supported shapes remain retained.
- `particle_gles3_aabb_mode_layout/run.py`: **4/4** checks passed.

## `particle-contract-fixture-recheck-20261106`

- `particle_renderer_setter_contract/run.py`: **10/10** checks passed.
- `particle_scale_over_velocity/run.py`: **11/11** checks passed, including equal-bound and clamped scalar cases.
- `particle_aabb_flags/run.py`: **17/17** float checks and **17/17** double checks passed.
- `particle_instance_uniform_guard/run.py`: **3/3** checks passed.

## `particle-fixture-suite-syntax-recheck-20261106`

- **35/35** runner files compiled successfully.
- `git diff --check` passed for the 2026-11-06 particle receipts.

## `particle-gles3-resource-fixture-recheck-20261106`

- `particle_gles3_process_retry/run.py`: **11/11** checks passed.
- `particle_gles3_update_queue/run.py`: GREEN; failed GLES3 particle buffer work is deferred to a later update.
- `particle_collision_setter_bounds/run.py`: **14/14** checks passed.
- `particle_draw_pass_dependency/run.py`: **8/8** checks passed.
- `particle_emit_input_bounds/run.py`: **4/4** checks passed.

## `particle-heightfield-fixture-recheck-20261106`

- `particle_collision_scene_bounds/run.py`: **9/9** checks passed.
- `particle_heightfield_size/run.py`: **18/18** float and **18/18** double checks passed.
- `particle_gles3_heightfield_size/run.py`: **8/8** checks passed.
- `particle_rd_heightfield_extents/run.py`: **11/11** checks passed.
- `particle_rd_heightfield_failure/run.py`: **2/2** checks passed.
- `particle_gles3_heightfield_failure/run.py`: **10/10** checks passed.

## `particle-lifecycle-fixture-recheck-20261106`

- `particle_buffer_retry/run.py`: **26/26** checks passed.
- `particle_deferred_retry/run.py`: **2/2** checks passed.
- `particle_epoch_reset/run.py`: **8/8** checks passed.
- `particle_rd_cleanup/run.py`: **2/2** checks passed.
- `particle_collision_uniform_retry/run.py`: **7/7** checks passed.

## `particle-processing-fixture-recheck-20261106`

- `particle_process_material_range_bounds/run.py`: **10/10** checks passed.
- `particle_process_progress/run.py`: **51/51** checks passed.
- `particle_storage_setter_bounds/run.py`: **39/39** checks passed.
- `particle_update_queue/run.py`: **3/3** checks passed.

## `particle-rd-compatibility-recheck-20261106`

- `particle_gles3_unsupported_field/run.py`: Compatibility collision admission passed; unsupported 3D fields are skipped while supported shapes remain retained.
- `particle_rd_heightfield_extents/run.py`: **11/11** checks passed.
- `particle_rd_heightfield_failure/run.py`: **2/2** checks passed.
- `particle_buffer_retry/run.py`: **26/26** checks passed.

## `particle-setter-dependency-fixture-recheck-20261106`

- `particle_storage_setter_bounds/run.py`: **39/39** checks passed.
- `particle_renderer_setter_contract/run.py`: **10/10** checks passed.
- `particle_collision_setter_bounds/run.py`: **14/14** checks passed.
- `particle_emit_input_bounds/run.py`: **4/4** checks passed.
- `particle_draw_pass_dependency/run.py`: **8/8** checks passed.

## `particle-trail-setter-fixture-recheck-20261106`

- `particle_trail_binding/run.py`: **16/16** checks passed.
- `particle_gles3_lifetime_split/run.py`: **5/5** checks passed.
- `particle_gles3_draw_pass_count/run.py`: **4/4** checks passed.
- `particle_gles3_setter_bounds/run.py`: **13/13** checks passed.
- `particle_collision_base_size/run.py`: **6/6** behavior checks and **4/4** source checks passed.

## `particle-update-history-fixture-recheck-20261106`

- `particle_gles3_history_allocation/run.py`: both zero-name rollback/retry paths passed.
- `particle_deferred_retry/run.py`: **2/2** checks passed.
- `particle_update_queue/run.py`: **3/3** checks passed.
- `particle_process_material_range_bounds/run.py`: **10/10** checks passed.

## Common evidence boundary

- These receipts record source-extraction, fixture, syntax, or harness evidence as stated by each entry.
- They do not, by themselves, prove live GPU behavior, product six-link acceptance, or completion of a full engine build.
- Production source was not changed by the 2026-11-06 recheck commits.
