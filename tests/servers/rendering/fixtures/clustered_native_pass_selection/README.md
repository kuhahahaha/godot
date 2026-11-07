# Clustered pre-list native pass source

Real native framebuffer format, pass version and pass-invariant color flags are selected before each draw-list begin and consumed by the original draw pipeline. The common wrapper and direct material/UV2 entries share the fixed-size selection. Per-surface lightmap/forward GI and UV2 offset/wireframe changes stay in their original branches. This creates no output grant or owned prefix admission.

Independent source review checked all eight passes, original twelve color combinations, multiview limits and all consumer entries; exhaustive flags0..65535 matched the original explicit cases. Four new CPU cases exercise independent pass/color/inclusion/failure-preservation expectations.

Actual parent final build0/13.43s; focused27cases/841assertions includes all four new cases; configured full2263cases/584864assertions0. Same host5c806589c961a3a360790c29cf138164c801d66f1a385f5ce2b8dd9b60140852 passed realMetal Mobile/Forward+ registered-camera24checks each, original viewport22each/four RGBA comparisons, numeric heightfield16 and legacy-neighbor12cases. Full commands, original logs/PNG, source hashes and build metadata are retained in ../camera_native_propagation/evidence_20261107/. Helper-only CPU tests are not GPU draw proof. No new external Sprite draw or native retirement authority is claimed.
