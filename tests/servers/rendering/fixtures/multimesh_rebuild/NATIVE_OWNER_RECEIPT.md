# K02 M03/M11 native owner constituents

The test cases in `tests/servers/rendering/test_multimesh_rebuild.cpp` now exercise
original MeshStorage and RenderingDevice creation, transfer submission/wait and
deferred disposal with a CPU memory driver. The full MultiMesh filter passed
7 cases / 2,593 assertions, exit 0. Five cases are new.

M03 checks 32 different native command BufferIDs: each remains physically live
after RID invalidation, returns exactly once through the original deferred free
path, and leaves no GPU buffer live. A repeated disposal has no extra return.

M11 separately refuses instance/command GPU creation, command initialization
mapping, and existing-command update staging mapping. Each checks original
failure notification, cleared current command, prefix/deferred cleanup and
same-argument public retry restoring the correct native command words.

Run after building with tests enabled:

```sh
bin/godot.macos.editor.arm64 --headless --test --test-case='*[MultiMesh]*'
```

Independent source review found no blocker. This is a controlled native-driver
constituent, not real GPU completion, renderer draw, pixel evidence, M12 or full
product acceptance. The mesh owner contains fixed draw metadata, and fence
responses are controlled. `NATIVE_OWNER_RECEIPT.json` binds the actual editor,
test source and measured counts; no production behavior was changed.
