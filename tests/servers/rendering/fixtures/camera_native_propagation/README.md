# Native camera propagation through actual RD scene rendering

This tests-only fixture requires a `tests=yes` build with
`module_particles_runtime_tests_enabled=yes`. It runs real Metal Forward Mobile
and Forward+; headless/dummy and Compatibility are not substitutes.

From the repository root, after the unified build:

```sh
python3 tests/servers/rendering/fixtures/camera_native_propagation/run_native.py \
  --binary /absolute/path/to/tests-enabled-godot \
  --capture-dir /absolute/path/to/evidence/camera-native-propagation
```

The runner records each exact command, complete process output, real renderer
and device startup line, process exit, and the complete JSON report. Each backend
writes three 128x128 PNGs and `run.log` beneath the evidence directory. Every
image must have a nonuniform raw RGBA8 readback, a SHA256, and actual draw calls;
the moved camera must change the first/second image hashes. Both backends must
exit zero within 90 seconds, emit exactly one valid report, and emit no ERROR or
WARNING diagnostics. All required checks and any additional reported checks must
be true. Missing devices or missing fixture methods fail this required gate.

The host creates a normal SubViewport, BoxMesh and Camera3D. It first draws the
unregistered viewport and observes its null capture cache on RT. It then
registers a fixed test-only presentation key (view_id `0x43414d`, epoch 1), draws
with sequence 1, moves the camera, registers sequence 2 and draws again. Each
UPDATE_ONCE draw uses `RenderingServer.force_draw`; every probe is queued on the
same RT after that draw and acknowledged before advancing. Images read the
actual viewport GPU output. The scene/render lifecycle stays active throughout
capture, retention, invalidation and viewport free.

The probe reads `viewport_owner` and requires `captured_camera` to be nonnull
**before** copying any Ref. It never calls the lazy
`viewport_get_camera_capture_ref` getter, constructs CameraData/RenderDataRD,
invokes render_scene directly, or uses a fake completion. Thus removing the
production RD retain call makes the production_cache_nonnull checks fail. It
checks actual source viewport, full token, presentation sequence, camera
transform/projection and immutable old payload, plus a different new Ref/token.
The old Ref stays alive across invalidation and actual viewport free, and is
finally released on RT. Original fixture APIs and existing acceptance remain
unchanged.

This is a native source propagation test. Test-only presentation registration
does not issue a view, grant, accepted authority, external permission, or any
claim about an external consumer's admission. A stored source RID is compared as
identity after viewport free, never dereferenced as a live viewport.

Source-only authoring does not constitute GPU acceptance: run the above command
against the freshly built binary and preserve its full output.
