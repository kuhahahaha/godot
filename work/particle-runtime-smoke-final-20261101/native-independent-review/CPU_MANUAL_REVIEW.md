# Independent scoped CPU manual-request review

2026-11-01. Reviewer: native_ab_seam, independent from the Astra candidate author. Current production CPU2D/CPU3D/manual source fixture match the authored candidate byte-for-byte. Fingerprints and exact evidence checks are in `REVIEW.json`.

Conclusion: **no blocking implementation finding in the two minimal CPU changes**. The retained real lifecycle probe credibly exercises the required public behavior; no production/test source change or additional native run was made by this reviewer. No scons build was started.

## Source contract checked

- The public request setter only enqueues the two finite pending durations and invokes update. An unprocessable finite duration remains pending after update refusal; this is deliberately not a claim that every field, including pending inputs, is unchanged.
- Positive manual emit alone writes `emitting=true`, `active=true`, and `set_process_internal(true)` before the real simulation loop. This matches the necessary state restoration of `_set_emitting()` without calling it recursively when time==0 and the request is still pending. Real `_particles_process()` can clear node active after particles finish; it does not synthesize node active on birth.
- The residual-only branch never enables active/internal scheduling. It advances the requested residual interval with emitting=false and preserves an initially stopped system's scheduler state. There is no unconditional revival added to residual/preprocess paths.
- All used request stages are checked for finite/representable request time and float-decrement progress before enabling redraw. Both emitted and residual durations are checked before starting either phase, so a bad residual cannot partially execute the emit prefix.
- 2D `_refresh_interpolation_state()` now follows admission checks; it can mutate interpolated_follow/internal physics scheduling only after admission succeeds. It still executes before the existing inactive/idle cleanup path. The initial invisible/empty-particle early return retains its earlier independent draw-disable behavior and does not consume a request.
- The normal fixed-FPS/nonfixed-FPS tail, frame remainder, speed-scale save/restore, per-particle birth/death, one-shot cycle logic and finished emission remain owned by the existing real simulation. The change does not replace them with a test model.

## Source fixture honesty and independent run

The updated source harness extracts the actual production `_update_internal()` body. Its recording `_particles_process` no longer sets node active=true: the prior synthetic assignment is absent. The harness clearly labels itself admission/scheduling coverage, not full particle lifecycle/rendering.

This reviewer ran the current source fixture once: all four 2D/3D × float/double typedef variants passed 22/22, with the intentional progress-refusal diagnostics. These are four extracted-control variants, **not** four engine builds or native lifecycle runs. The small harness checks private modeled flags and ordering; actual lifecycle still needs the separate real nodes below.

## Real native lifecycle evidence checked independently

Read the entire `astra-review/probe/lifecycle_probe.gd`, `manual-lifecycle.json` and raw `.log`, not merely the author's summary. Recomputed the script, host executable and controlled-build hashes. Both current CPU source files match the pinned engine's recorded source window. The tested immutable ordinary headless editor executable is `ec55001baf673a0d9ae29b68ec5fe44d21098e5da8e7a5e183bc515add631e4d`, metadata `1387afca42e958e70b169bf7bf54fdf003abb11f6fd73cb789317298b867244a`.

The probe instantiates actual CPUParticles2D/3D nodes, enters them in the actual SceneTree, and observes actual frames, processing flags, finished signals, and the real RenderingServer frame_pre_draw connections. It never assigns private active or fakes node scheduling/signal connections. Fixed host FPS=60, particle fixed FPS=30, lifetime1, amount16 and fixed seed7 make the bounded lifecycle cases reproducible. The timeout bound is not the success criterion: each case explicitly requires stopped processing, emitting=false and exactly one finished signal.

Observed and assertion-backed cases:

- Each dimension's combined emit0.25/residual0.125 starts real processing, naturally stops after 54 frames, and retains exactly one finished signal after three extra frames.
- Each dimension's one-shot emit0.25 starts real processing, naturally stops after 102 frames, and retains exactly one finished signal after three extra frames.
- Actual residual-only calls on empty systems are followed by three real frames and assert processing=false/emitting=false. These assertions are in the executed, hash-matched script even though they do not create separate output rows.
- Actual running systems reject duration16777216 with before/after processing/emitting/draw/internal-physics observations exactly equal in both dimensions. The two raw ERR_FAIL diagnostics are expected and occur exactly at these calls; rc0 alone would not establish this.
- For 2D the node first uses global coordinates; tree interpolation is enabled after the before snapshot and immediately before the refused request. Refusal leaves physics_internal=false. A later admitted clear request explicitly asserts physics_internal=true, providing a positive control that the changed interpolation policy was real and would have an effect. There is no intervening frame between the refusal and its snapshot to obscure ordering.

The final result contains six rows, an empty failures array, rc0 and exactly two expected progress-refusal diagnostics. Source/host/script/hash consistency checks all pass. This is credible actual CPU node lifecycle and admission evidence. It does not inspect particle pixels/private particle buffers, certify Metal/GPU behavior or imply ANGE/provider/owner acceptance. Runtime double-precision engine behavior is not separately claimed.

## Boundary and integration

The earlier native GREEN probe in `manual-native-green.json` only covered scheduling after combined requests and refused redraw setup. The lifecycle probe above closes the previously identified natural-stop/one-shot/residual/interpolation coverage gaps; it is not equivalent to the extracted 22/22 layer.

The broader 14 source fixtures, 14/75 node C++ assertions and 185/3141 Qualification assertions were reported by the main coordinator; this scoped review does not re-run or independently extend those separate claims. Commit the actual source/test changes and retain these layered receipts without promoting unrelated native/GPU results. No further change is requested by this reviewer for this minimal patch.
