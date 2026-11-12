# ANGE Godot native development source

This branch materializes the entire source cohort used by the validated private 634a559c… Metal host, starting from clean team ancestor 20a14abb7a0293961f1ba136f3319207c1e20342. It contains the fixes evidenced by C commits 77b77190f7 (authenticated staged acquire refusal association), 5f2a6b47af (Mobile transparent pre-list preparation), and 04b95db1a7 (Forward+ transparent preparation). Those three evidence commits alone did not reconstruct the complete engine.

`HISTORICAL_SOURCE_INPUTS.json` pins all 15,743 original source inputs. `PUBLICATION_NORMALIZATION.json` records the sole raw-byte change in those inputs: one inert historical CRLF log becomes LF according to Git attributes. Windows-only CRLF source files retain their declared checkout form. `HISTORICAL_ADOPTION.json` and `INDEPENDENT_OTHER_CANDIDATES.json` enumerate all 69 ordered private patches and formal35, inherited production, fixture and diagnostic changes. `HISTORICAL_INTAKE.md` and `INDEPENDENT_AUDIT.json` preserve the exhaustive historical audit. Source in this commit, rather than a superseded patch's filename, determines the active implementation.

Five production paths contain TESTS-only diagnostic instrumentation: Mobile renderer, external_output_packet.cpp, mesh_storage.cpp, rendering_device_immutable_hold.cpp, and rendering_device_output_read.cpp. These are explicitly part of the delivered source. The full audit also lists 58 production paths inherited from the authenticated original dirty baseline. This is a development branch with tests enabled; no claim is made that those private candidates have all passed product-level acceptance.

Build from this exact commit using the repository's original build path:

```sh
scons -Q platform=macos arch=arm64 target=template_debug tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes disable_path_overrides=no -j2
```

After a successful link, keep the executable, its `.controlled_build.json`, `core/engine_build_selection.gen.json`, `core/engine_build_identity.gen.json`, and `core/engine_build_identity.gen.cpp` together. The original Core generator derives the stamps from actual link artifacts and build tools/recipes; never edit these sidecars to change cohort.

The source commit precedes the branch's validation build. To retrieve the actual build selection and receipt pins for this fixed commit:

```sh
git fetch origin refs/notes/ange-native-builds:refs/notes/ange-native-builds
git notes --ref=ange-native-builds show HEAD
```

Build notes do not alter the compiled source commit. Local absolute artifact paths in the note are delivery evidence, not downloadable binaries; consumers rebuilding elsewhere must derive their own selection through the original build path.

The matching interface increment is in `../ange_native_sdk/`: schema, original generators, generated C header and both loader products. Its `cohort/` selection is explicitly the preserved historical 634a… host. A build of this branch has a new Git-version-derived Core/compiler selection and must use its newly generated build record. The SDK increment does not include a full class API export or a prebuilt GodotCPP archive. A must retain its selected class API and ordinary GodotCPP tool/source tree, regenerate through the original path and link its actual extension/archive against the new host selection.

The historical real Metal checks establish native component behavior: ordinary opaque and half-blended transparent output, actual 32-byte color and 192-byte transform GPU copies, same-camera renderer-owned draw, sole close, and physical retirement without a following draw in the accepted positive cases. The bank refusal case has its own raw0 retirement contract and uses maintenance draw(false) calls. It is not a no-following-draw claim. Full ANGE typed product acceptance remains open until A supplies the real presentation pass intent and a matching rebuilt extension; the current issuer reports `output.presentation.pass_intent` NOT_READY. No synthetic authority or second close is introduced.
