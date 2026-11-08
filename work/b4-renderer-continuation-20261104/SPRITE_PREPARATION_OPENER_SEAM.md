# Sprite preparation opener seam

Source audit identifies the exact lifecycle boundary for the next implementation slice.
`Registry::production_output_acquire` binds each finite preparation row and allocates the
slot while the original action remains `RESERVING`/the slot is `RESERVED`; it transitions
the action to `BOUND` only after aliases and child SET creation succeed (`core/extension/
gdextension_production_output.cpp`).

`ParticlesStorage::_external_pipeline_prepare_owned_prefix` authenticates that same
reserved slot, row address, lease and physical context, then obtains the owned group and
opens the row. Forward clustered/mobile `external_pipeline_prepare` consume the opened
row and perform exact material/shader/geometry prefix preparation. They do not issue an
owner grant and must not be called after the action is BOUND.

The current acquire DTO has no framebuffer/subpass/shader-version/specialization selection
and no renderer surface-selection borrow. Therefore inserting a call from the current
Core acquire path would fabricate renderer authority or an incomplete pipeline request.
The correct next owner-owned continuation is a renderer-side call during the original
RESERVING opportunity, carrying an authentic `ExternalSurfaceSelection` and
`ExternalPacketPipelineRequest`; it must preserve the row closed if selection or prefix
preflight fails. No public Instance192 gate, E09 issuer, or BOUND-pin replay is added by
this seam audit.

This receipt is source evidence, not a production implementation or visible-draw claim.
