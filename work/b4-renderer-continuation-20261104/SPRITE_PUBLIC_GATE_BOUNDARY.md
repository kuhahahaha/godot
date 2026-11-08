# Sprite public gate boundary audit

Current source has the renderer consumers but no production caller:

- `ParticlesStorage::particles_external_surface_acquire` returns `ERR_UNAVAILABLE`.
- `particles_external_view_acquire`, `particles_external_resolve`, and
  `particles_external_end_draw_list` also return `ERR_UNAVAILABLE`.
- Forward clustered/mobile `external_pipeline_prepare` only consume an already
  authenticated `ExternalSurfaceSelection`, `ExternalPipelinePreparation`, and
  exact `ExternalPacketPipelineRequest`; they do not issue owner authority.

The private owner tests inject rows and pipeline groups to exercise custody and
retirement. They do not establish a public Sprite positive. The original
Acquire path lacks framebuffer/subpass/shader-version/specialization selection,
so D cannot fill these fields or invoke the renderer opener after the action is
BOUND. The next implementation must import the original selected surface and
pass request during the authentic RESERVING/RESERVED physical callback, then
carry the same row through geometry/material transfer, list recording, read
lease return, and no-next-draw closure.

No public gate, issuer, renderer scheduler, or fallback counter is added by this
boundary audit.
