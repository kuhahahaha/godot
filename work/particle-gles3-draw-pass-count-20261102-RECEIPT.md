# GLES3 particle draw-pass count setter receipt

Date: 2026-11-02

`ParticlesStorage::particles_set_draw_passes()` rejects counts outside 1..4 before vector resize and notifies `DEPENDENCY_CHANGED_PARTICLES` after a valid resize. This prevents negative count conversion and keeps render dependents synchronized when the pass count changes.

A source-extracted C++ harness executes the production setter against a minimal owner/dependency model: pre-change HEAD is RED (0/4); current source is GREEN (4/4), including valid counts, invalid counts and invalid RID. The staged patch contains only this setter, harness and receipt. `git diff --cached --check` passed. This does not qualify a live Compatibility draw-pass rendering frame; the prior Compatibility lifecycle probes cover neighboring normal behavior only.
