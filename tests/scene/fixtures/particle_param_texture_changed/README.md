# Particle parameter texture changed synchronization

`ParticleProcessMaterial::set_param_texture` now subscribes each sampler-backed parameter slot to its wrapper's deferred `changed` signal. The callback reads the current slot reference and refreshes only that sampler RID; it does not queue a shader variant change. Non-sampler parameter slots retain their existing no-op behavior.

Each sampler slot uses a distinct template method. Godot signal lookup compares the callable's base and ignores bound arguments, so using one method with `.bind(parameter)` loses subscriptions when one wrapper is assigned to multiple slots.

Run the real engine fixture with `python3 tests/scene/fixtures/particle_param_texture_changed/run.py --godot /absolute/path/to/godot --output /absolute/path/to/evidence`. It runs on Metal Forward Mobile and reads the raw material Variant on the actual main RD render thread. The 15-slot matrix covers shared wrappers, same-reference refresh, replacement/detach, queued old callbacks, empty RID versus NIL, render-thread notifications, and destruction before drain. A separate cross-slot case requires two subscriptions and verifies replacing/clearing one leaves the other live. The three non-sampler slots remain unsubscribed.

Acceptance requires exactly 973 checks and 248 observations in the expected order, correct A/B RID associations, a single result marker, exit zero, stable binary/fixture hashes, and no engine/script/leak diagnostics. `run_engine_controls.py` exercises malformed payload and process controls, including binary mutation and leak output. The source harness covers 22 scheduling/binding assertions. Its six runner controls distinguish behavioral failure (exit 1) from compilation failure (exit 2).

Current real-engine evidence is in `work/param-texture-sync-implementation-20261112/`. The old bind-once binary fails; the intermediate bound-callback implementation independently fails the cross-slot checks; distinct-method callbacks pass. These results prove material RID association on a real Metal main RD. They do not prove particle shader sampling, visible pixels, or native physical retirement.
