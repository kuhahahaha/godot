# Particle color initial ramp changed regression

This fixture verifies one contract: `ParticleProcessMaterial.color_initial_ramp` keeps the material parameter associated with the current `Texture2D` wrapper RID when that wrapper emits `changed`. It checks material parameter association only; it does not claim particle pixels, GPU sampling, or visual output.

Run the strict actual-engine oracle with a real Metal main RenderingDevice:

```sh
python3 tests/scene/fixtures/particle_color_initial_ramp_changed/run.py \
  --godot bin/godot.macos.template_debug.arm64 \
  --output work/color-initial-ramp-run
```

The runner uses a temporary project with `.godot/global_script_class_cache.cfg` containing `list=[]`, requires Metal Forward Mobile startup, rejects all engine/script errors, enforces one JSON `PARTICLE_COLOR_INITIAL_RAMP_RESULT` marker, a zero process return code, and a watchdog. The fixture uses valid distinct A/B `ImageTexture` RIDs and keeps the raw NIL versus RID Variant distinction in its snapshots. It covers shared wrappers, A→B reassociation, same-reference immediate refresh, deferred delivery, duplicate prevention, replacement and old-resource detach, queued old notifications, empty-wrapper RID and recovery, render-thread emission, destruction before deferred drain, and no shader-variant request from content-only changes.

`run_source.py` is supporting method-level evidence only. The authoritative acceptance scope is the actual main-RD run.

This fixture is copied from the hardened color-ramp oracle, retaining 63 actual-engine checks, 16 raw observations and 36 malformed/process runner controls. Initial-ramp source RED/GREEN evidence is recorded under `work/color-initial-ramp-sync-20261111/`. Root owns real-engine RED/GREEN runs; source-shell evidence alone is not engine acceptance.
