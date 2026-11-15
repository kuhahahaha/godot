# Particle color ramp changed regression

This fixture verifies one contract: `ParticleProcessMaterial.color_ramp` keeps the material parameter associated with the current `Texture2D` wrapper RID when that wrapper emits `changed`. It checks material parameter association only; it does not claim particle pixels, GPU sampling, or visual output.

Run the strict actual-engine oracle with a real Metal main RenderingDevice:

```sh
python3 tests/scene/fixtures/particle_color_ramp_changed/run.py \
  --godot bin/godot.macos.template_debug.arm64 \
  --output work/color-ramp-run
```

The runner uses a temporary project with `.godot/global_script_class_cache.cfg` containing `list=[]`, requires Metal Forward Mobile startup, rejects all engine/script errors, enforces one JSON `PARTICLE_COLOR_RAMP_RESULT` marker, a zero process return code, and a watchdog. The fixture uses valid distinct A/B `ImageTexture` RIDs and keeps the raw NIL versus RID Variant distinction in its snapshots. It covers shared wrappers, A→B reassociation, same-reference immediate refresh, deferred delivery, duplicate prevention, replacement and old-resource detach, queued old notifications, empty-wrapper RID and recovery, render-thread emission, destruction before deferred drain, and no shader-variant request from content-only changes.

`run_source.py` is supporting method-level evidence only. The authoritative acceptance scope is the actual main-RD run. The pre-change RED was recorded against revision `06831834fbb296f5a55c117e28ffa9622142e83b` and the pinned binary SHA-256 in `work/resource-sync-next-slice-20261111/implementation-evidence/original-engine-red.json`; source RED is preserved in `implementation-evidence/original-source-red.log`.
