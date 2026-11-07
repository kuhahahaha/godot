# SceneOnly receiver across real viewport draws (opt-in)

This fixture is an integration repro and a ready consumer of the pending native
SceneOnly render-routing seam. It is not part of a default test suite. Build a
real editor with `tests=yes module_particles_runtime_tests_enabled=yes`; run
`run_native.py --binary /absolute/editor --capture-dir /absolute/evidence`.
The runner uses real Mobile and Forward+ and rejects every ERROR/WARNING.

The script explicitly flushes Node3D transform notifications for the mesh,
sprite, light and each camera pose through `force_update_transform()` before
rendering; force_draw alone does not flush SceneTree transform notifications.
The script creates one ordinary box mesh, one billboard Sprite3D, a shadow-enabled
directional light and a 128×128 SubViewport. It takes actual GPU image readbacks
and visible draw counts for top and side camera views before connection. It then
uses the existing adopted Callable, original protected RECORD and actual draw's
typed phase callback to install an original generation root. A SCENE_ONLY native
receiver is attached to that SAME viewport's world and its original committed
query is checked against a real sphere source. Receiver and root remain alive
through both after-view draws and their actual image readbacks. Each comparison
requires two visible draws, nonuniform pixels in both actual images and
identical RGBA8 bytes; four PNGs and raw-RGBA SHA256
values are emitted per renderer. stage4 is readiness only, never GPU completion.

The box uses an explicit opaque white unshaded test material to avoid unrelated
lighting variation; the directional light's shadows and the mesh's shadow casting
remain enabled. To compare two top/side GPU readbacks without creating a managed
receiver, run the diagnostic control directly (use a fresh capture directory):

```bash
ANGE_SCENE_VIEWPORT_BASELINE_ONLY=1 \
ANGE_SCENE_VIEWPORT_CAPTURE_DIR=/absolute/baseline-mobile-evidence \
  /absolute/editor --display-driver macos --rendering-method mobile --rendering-driver metal \
  --path tests/servers/rendering/fixtures/particle_scene_viewport_invariance_native \
  --script res://run.gd
```

Repeat with `--rendering-method forward_plus` and a separate capture directory
for Forward+. This emits `mode: baseline-only` with the same exact RGBA/draw-count
comparisons but no managed root checks; it is a setup stability control, not a
formal managed acceptance run. The normal runner still requires all 18 lifecycle
checks and emits `mode: managed`.

After observation, the original receiver/source are freed, future drawing stops,
and the same waiting-RECORD refusal mailbox, unregister, Callable free/holder ACK,
generation OBSERVE/RELEASE and enrollment cleanup checks run. The existing managed
55-check default route is unchanged. The borrowed viewport world is freed only
after all original owner cleanup has returned.

Current expected RED: ordinary renderer/dependency/cull code still sends a live
external SCENE_ONLY receiver to legacy particle getters/process requests. The
fixture leaves it visible, leaves shadows enabled, and retains it across actual
camera draws so those errors are exposed, not suppressed. Only the original
production owners may add the missing routing; this fixture modifies no owner or
dispatch API and never invokes scene_phase_dispatch directly.

The native test substrate is not a public Scene connection issuer. Output reports
`external_scene_connection: not-present` and `pair_membership: not-observed`.
No C05/E09 connection, ordinary draw receipt/source-identity join, owner-use proof
or complete H25 acceptance is claimed. A future clean pixel comparison still
requires those separate owner facts before full contractual acceptance.
