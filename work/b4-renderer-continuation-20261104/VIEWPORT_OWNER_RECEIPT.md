# RendererViewport production owner boundary receipt

Date: 2026-11-08  
Worktree: `codex/b4-renderer-continuation-20261104`  
Scope: source audit and delivery boundary for the existing
`viewport_register_presentation_key` / camera-capture APIs.

## Result

No production owner/caller currently exists for `ViewPresentationKey`. The
existing APIs remain usable by the renderer, but there is no typed
`RendererSceneCull`/`Viewport`/`World` owner that can supply an authoritative
`view_id`, `view_epoch`, and `present_seq`. Therefore this slice intentionally
adds no registration call, no registry, no ABI, and no synthetic fallback key.

## Source evidence

* `servers/rendering/renderer_scene_cull.cpp:2966-2973` is the real production
  camera seam. After ordinary/XR `CameraData` construction,
  `render_camera()` calls `viewport_begin_camera_capture(viewport, camera_data)`.
  The call can only issue a valid token when the viewport was already
  presentation-registered; otherwise it clears `source_viewport`.
* `servers/rendering/renderer_scene_cull.cpp:3911-3920` is the real post-scene
  owner path. It stages the exact captured token/data when a presentation key
  exists, otherwise it advances ordinary viewport previous-camera history.
  This is camera history custody, not a presentation-key issuer.
* `servers/rendering/renderer_viewport.cpp:1357-1399` confirms that capture
  derives `view_id`/`view_epoch` from the pre-existing viewport key and stores
  the complete `CameraData`; it never creates an identity.
* `servers/rendering/renderer_viewport.cpp:1433-1457` validates and stores a
  caller-supplied key. The only non-test definition/call-site search result is
  this method itself; no production caller under `servers/`, `scene/`, or
  `core/` invokes it.
* `modules/particles_runtime_tests/particles_scene_pairs_test_fixture.cpp:250-260`
  is the sole caller found in the checkout. It explicitly uses the fixture
  constants `0x43414d` and epoch `1`; those are test identities and cannot be
  promoted to production authority.

## Owner prerequisite / handoff

An acceptable follow-up must first identify the original selected-view owner
and its generation/request custody (viewport RID, camera/eye or pass identity,
render-buffer incarnation, view epoch, and presentation sequence). That owner
must register the exact key before the `render_camera()` seam and revoke it on
owner/viewport teardown. The capture must retain the original owner association
through the existing output record/read lifetime. A copied key DTO, raw RID,
global token map, fixture constants, empty bounds, or a token-only callback is
not sufficient. Until that typed provider is available, wiring this endpoint
would manufacture authority and is deliberately left incomplete.

## Verification

The following read-only source checks were run from this worktree:

```sh
rg -n "viewport_register_presentation_key" servers modules --glob '*.{cpp,h}' --glob '!modules/particles_runtime_tests/*'
rg -n "viewport_begin_camera_capture|viewport_stage_camera_data|viewport_adopt_camera_data" servers/rendering --glob '*.{cpp,h}'
git diff --check -- work/b4-renderer-continuation-20261104/VIEWPORT_OWNER_RECEIPT.md
```

The first command returned only the `RendererViewport` declaration/definition;
the fixture is the only external caller. The second command returned the real
pre-cull and post-render paths listed above. No full host rebuild was started.

## Explicit non-claims

This receipt does not claim a production issuer, selected-eye association,
`present_seq` progression, host/world ownership, output-record join, ABI
extension, or runtime rendering coverage. The unresolved gate is the authentic
typed selected-view owner and its original generation/request caller.
