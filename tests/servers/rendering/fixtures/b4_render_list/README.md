# B4 render-list admission regression

Build a `tests=yes` macOS arm64 editor including this fixture, then run:

```sh
python3 tests/servers/rendering/fixtures/b4_render_list/run.py \
  --engine /absolute/path/to/godot.macos.editor.dev.arm64 \
  --generated-root /absolute/path/to/the/build/source
```

The generated root contains `core/extension/gdextension_interface.gen.h`. The
runner compiles the small guarded test extension using `clang`, creates an
isolated temporary Godot project and runs the real Metal Forward Mobile and
Forward+ renderers. It returns nonzero for a missing/duplicate assertion marker,
wrong renderer, engine or extension failure, unexpected error diagnostic or
timeout. Use a separate build for other platforms; this runner currently
supports macOS arm64 only.

The extension invokes a test-only entry built into the editor. The fixture
constructs real native mesh, shader, material and geometry objects, then varies
only the generation in their surface caches and injects a controlled external
packet. It directly invokes the renderers' private list fill through a
`TESTS_ENABLED` friend. This exercises list admission, color/alpha/motion
membership, shadow append preservation, ordinary behavior, first rejection,
retired and empty input, and recovery. It does not establish an upstream
product issuer, E12 material lease, cross-pass transaction, GPU draw or
completion. All temporary geometry and list entries are released before
returning to the renderer.
