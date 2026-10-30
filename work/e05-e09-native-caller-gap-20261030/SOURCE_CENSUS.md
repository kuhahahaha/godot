# E05 → E09 native submit/completion/readback/release source census

Baseline Godot HEAD is recorded in `GODOT_HEAD.txt`; the explicit ANGE DSO and
ANGE source HEAD are recorded in `DSO_SHA256.txt` and `ANGE_HEAD.txt`.

## Actual call chain

1. Godot `src/register_types.cpp:materialize_scene_host` obtains the original
   `RenderingServer`/main `RenderingDevice`, creates a local device, and invokes
   `HostAttachmentInstaller::install_main_storage` before
   `HostAttachmentInstaller::bind_main_device`.
2. ANGE `src/godot/particle/host_native_device.cpp:40-83` checks exact main-device
   identity and render-thread ownership, then calls
   `install_main_storage_candidate`.
3. `host_attachment.cpp:539-670` binds the original SceneReady device session,
   requires the original main owner, and calls `read_storage_owner_inputs`.
   That reader currently returns `require_storage_owner_inputs()` unconditionally
   (`host.storage_owner_transport`, E05, expected 1 actual 0). No storage,
   qualification bridge, native port, or E09 child is published after this refusal.
4. If the missing owner transport were supplied, the existing path would install
   `MainRDStorageTransport` and `ParticleAttributeStorage`, then
   `bind_main_device` would create `GodotNativeUnitPort` and bind the loader.
5. The next real E09 entry is `E09NativeRecorder::start`
   (`src/godot/particle/e09_native_recorder.cpp:204-252`): it requires the
   qualification scope/native hold, original loader store, exact main RD on the
   render thread, declared readback extent, and upload segment capacity. Its
   service path (`:430-660`) borrows E05 storage parts, validates native shader /
   pipeline identities, creates uniform sets, prepares a ticket, uploads through
   `qualification_upload_rt`, records the prepared dispatch, and later observes
   the same ticket before result/readback handling.
6. Godot's corresponding backend path is already present and typed:
   `_submit_transfer_worker` (`servers/rendering/rendering_device.cpp:8072-8104`)
   returns a `QueueCallReport` with submission serial; `_wait_for_transfer_worker`
   (`:8106-8153`) polls the exact serial and recycles only matching completion;
   `_end_frame` (`:9366-9425`) seals readback records and submits workers; async
   readback maps staging buffers and only calls the callback after the full result
   extent is copied (`:9680-9719`, `:9726-9789`).

## First missing input and boundary

The first missing input is not a Godot submit/completion/readback implementation.
It is the upstream E05 owner transport/source declaration consumed by
`read_storage_owner_inputs`. Without that authenticated owner/context, issuing a
storage allocation or E09 ticket would fabricate authority. The Godot-side native
submit, exact completion serial, same-request readback, and release paths are
reachable only after E05 installation and E06 binding succeed.

No Godot source change is safe in this slice. The current pair therefore remains a
production caller gap package, not a positive native or GPU result.
