# Particle native gap scan

Date: 2026-11-06

A read-only scan covered the current CPU/GPU particle scene nodes, RD/GLES3 particle storage, and particle collision sources.

## Findings

- The scan did not establish a new independently actionable runtime defect. Marker absence does not prove that all runtime requirements are implemented, and dirty files alone do not establish an active owner assignment.
- Four `FIXME` matches concern exponential integer editor property hints in CPU/GPU 2D/3D registration.
- Two `TODO` strings refer to the local pending-time variable `todo` in CPU3D and RD request processing; their surrounding comments explain the temporary speed-scale override.
- Two genuine implementation notes remain: RD uses `screen_hz = 60` when trails select a default fixed FPS (`particles_storage.cpp:2022`), and GLES3 hardcodes `MAX_GLOBAL_SHADER_UNIFORMS 256` (`particles_storage.cpp:56`). These were omitted from the earlier inventory. Their desired behavior and ownership still need contract review before implementation; this scan does not close them.
- The seven scanned production files are pinned by SHA-256 in `SOURCE_MARKERS.json`, with all eight marker lines retained. A changed source hash requires a fresh review.
- Core particle production files have existing dirty changes, so no new source patch was applied. Coordination is required before editing them.

This is a static ownership/gap scan only. It does not claim live GPU execution, product six-link acceptance, or a full engine build.
