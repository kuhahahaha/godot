# Native open-items revalidation R3 — 2026-10-29

This is a current read-only census after the E20 graph-path consumer and
path-length coverage commits. The native lane has landed bounded graph and
qualification ordering work, but it still has item-scoped owner/integration
seams; none of those seams blocks unrelated queue work.

Current identities:

- Godot HEAD is recorded in `COMMAND_OUTPUT.json`; editor binary is
  `bin/godot.macos.editor.dev.arm64`.
- ANGE HEAD and the source hashes for the issuer/binding census are recorded in
  `COMMAND_OUTPUT.json` and `SOURCE_HASHES.txt`.

Current bounded regressions are green:

- `*device_exit*`: rc0, 133 cases / 2,619 assertions.
- `[R02][Graph]*`: rc0, 8 cases / 109 assertions.
- `[Qualification]*`: rc0, 20 cases / 346 assertions.
- `[K03][P2]*`: rc0, 22 cases / 350 assertions.

The following native seams remain open and are kept fail-closed:

1. `require_main_rd_qualification()` is present in the godot-cpp binding and
   native tests, but the current production `register_types` census has no
   original owner caller. The main-RD admission flag therefore remains
   default-off.
2. `StageExecutor::start_native_qualification` and
   `ParticleProgramLoader::request_qualification` remain implemented
   definitions/delegation without a normal production preparation/record actor
   issuing the full sequence.
3. Graph handles, transitive path validation, path-length facts and command
   buffer mapping are now tested, but the full checked append → request-owned
   native issuer → completion settlement → same-request exact-range readback
   join is still not present in the current acceptance proof.
4. Post-SCENE SceneTree/Engine wrapper enrollment and final instance-binding
   disposal still require the original BINDING owner action before G1.

No fallback provider, synthetic authority, operation/completion/readback
association, Ready promotion, or G1 relaxation was introduced. This receipt
constrains only the corresponding native acceptance rows; independent ANGE
authoring/cache/provider work continues.
