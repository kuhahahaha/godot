# E05 teardown and retirement current validation — 2026-10-30

This package follows Godot `d9bf8cf5755` and ANGE `8e2f7d282`.

The current Metal pair still stops at `host.storage_owner_transport/E05/UNAVAILABLE` during guarded SceneReady installation. Source review of `HostAttachmentInstaller::install_main_storage_candidate()` confirms the failed original pair is retained in `failed_main_installation_` when rollback close is busy, and `HostControlService::close_main_binding()` drains provisional storage before retrying the original pair close. `LibraryHostAttachment::destroy_root()` refuses while that obligation remains. No fallback owner or inferred limits are added.

Targeted D-side checks:

- `bin/ange_test_host_storage '[host_storage_boundary]'`: rc0, 7 cases / 352 assertions.
- `bin/ange_test_host_storage '[e05_scene_transaction]'`: rc0, 1 case / 134 assertions.
- Godot `*[Qualification]*`: rc0, 102 cases / 1896 assertions.
- Godot `*[K03]*`: rc0, 44 cases / 1153 assertions.
- Godot `*[P01]*`: rc0, 91 cases / 9612 assertions.
- Godot `*[CallQueue]*`: rc0, 6 cases / 92 assertions.

The D-side retirement and teardown contract is independently green. The remaining runtime boundary requires the upstream authenticated E05 owner transport declaration; no safe Godot-only production change can issue it.
