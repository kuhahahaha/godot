# RendererViewport live presentation source audit — 2026-11-08

Scope: Stage2 live presentation provider / camera source handoff. Read-only audit; no production edit, ABI change, registry, build, or GPU run.

## Verified existing path

- `RendererSceneCull::render_camera` (`servers/rendering/renderer_scene_cull.cpp:2970`) calls `RendererViewport::viewport_begin_camera_capture(p_viewport, camera_data)` only after the actual CameraData (including XR views) is complete. The returned `CameraSourceToken` is copied into `CameraData.source_*`; failure clears `source_viewport`.
- `RendererSceneRenderRD::render_scene` (`servers/rendering/renderer_rd/renderer_scene_render_rd.cpp:1470`) copies the exact source tuple into `RenderDataRD`, then calls `retain_native_camera_source` only when a valid source viewport/token exists.
- `RenderDataRD::retain_native_camera_source` validates the full CameraData payload through `viewport_get_camera_capture` before lazily retaining the immutable `Viewport::CameraCapture` Ref. Token scalars alone do not authorize a payload.
- `RendererSceneCull::_render_scene` stages previous-camera data only when `viewport_get_presentation_key(p_viewport).view_id != 0`; ordinary viewports use the existing previous-camera path.

## Missing production owner/provider

`viewport_register_presentation_key` has no ordinary production caller. The only non-test call is `modules/particles_runtime_tests/particles_scene_pairs_test_fixture.cpp:259`; other occurrences are rendering tests. No Host/World/U08 owner currently supplies a live `(view_id, view_epoch, present_seq)` registration to an ordinary viewport.

The existing `ViewPresentationKey`, `CameraSourceToken`, `viewport_begin_camera_capture`, `viewport_get_camera_capture_ref`, and `RenderDataRD` retention are therefore consumer-side primitives. They do not create a provider identity, viewport ownership, render-buffer/eye association, or output lease. `present_seq` is not derived from the global frame counter and must come from the original live presentation owner.

## Required next owner contract

The original Host/World/runtime owner must provide a fixed, typed registration call that owns the presentation identity and lifetime, then explicitly call `viewport_register_presentation_key` on the render thread for the target viewport. The registration must carry the actual viewport/eye/render-buffer association and bounded in-flight/metadata limits from the owner’s real budget. The existing camera path can then consume the registration without new ABI tokens or guessed camera data.

A production implementation must connect this registration to the actual camera invocation and release/invalidate it at the owner’s presentation close boundary. It must also retain the same source owner through `RenderDataRD` consumers and last use; `CameraCapture` Ref retention alone does not establish output/reader leases or native completion.

## Explicit non-adoption

No fixture `ViewPresentationKey`, empty RID, unit bounds, global frame counter, or opaque view tag was promoted into production. No source change is safe until the original provider owner supplies the typed registration and its lifetime/lease contract.
