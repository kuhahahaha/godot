# C# native predelete and explicit Dispose regression

Run from a source checkout after the normal Mono engine/glue/.NET assembly build:

```sh
bash tests/scene/fixtures/mono_predelete_dispose/run.sh ./bin/godot.linuxbsd.editor.x86_64.mono
```

The Microsoft.NET.Sdk project reuses the source-tree GodotSharp Debug output and Godot.SourceGenerators Release output already built by modules/mono/build_scripts/build_assemblies.py. It does not fetch a different Godot SDK or rebuild the engine. The runner uses real CSharpScript objects and checks native free, managed explicit Dispose, synthetic cleanup notification, nested native cleanup, cross-owner explicit Dispose, signal disconnection and RefCounted native-hold return. An actual completion marker and the strict full engine log are both required. Native predelete must not emit the Object association-mutation error.

The baseline failure was observed in the real Mono editor Linux PCK export. Local dotnet remains unavailable. The actual Linux Mono CI job on a638a36536 passed the engine and managed builds, source generator tests, this fixture with MONO_PREDELETE_DISPOSE_OK, and the original project export. VALIDATION.json records the completed job, raw-log identity and evidence scope. The overall platform matrix has separate Windows fixture and SCU collection failures; this result does not certify those platforms or live GPU behavior.
