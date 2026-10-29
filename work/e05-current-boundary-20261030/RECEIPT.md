# Current E05 owner boundary and D-side retirement proof — 2026-10-30

## Fixed pair

- Godot source: `5629a017f1`; editor SHA256 `cd07b546dc34220448cf8580405c6ef8745758bccbfd51146a50439613d0bd57`.
- ANGE production source: `8e2f7d282`; DSO SHA256 `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.
- Renderer: Metal 4.0 Forward Mobile, Apple M3 Pro.

## Runtime boundary

The current fixed Metal pair loads the extension and refuses guarded SceneReady installation at the authenticated owner seam:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

Runtime rc is 2. `HostAttachmentInstaller::read_storage_owner_inputs()` deliberately returns `UNAVAILABLE` because the original owner transport declaration is absent. No D-side default, profile constant, diagnostic key, fixture value, ObjectID/RID inference, or synthetic transport is introduced.

## D-side independent closure

The renderer qualification retirement and resource-release negative paths remain independently green:

- `bin/ange_test_host_storage '[host_storage_boundary]'`: rc0, 7 cases / 352 assertions.
- `bin/ange_test_host_storage '[e05_scene_transaction]'`: rc0, 1 case / 134 assertions.
- Godot `*[Qualification]*`: rc0, 102 cases / 1896 assertions.
- Godot `*[K03]*`: rc0, 44 cases / 1153 assertions.
- Godot `*[P01]*`: rc0, 91 cases / 9612 assertions.
- Godot `*[CallQueue]*`: rc0, 6 cases / 92 assertions.

These tests prove waiting/refusal and exact retirement safety; they do not grant owner authority.
