# C# native predelete and explicit Dispose regression

Run from a source checkout after the normal Mono engine/glue/.NET assembly build:

```sh
bash tests/scene/fixtures/mono_predelete_dispose/run.sh ./bin/godot.linuxbsd.editor.x86_64.mono
```

The Microsoft.NET.Sdk project reuses the source-tree GodotSharp Debug output and Godot.SourceGenerators Release output already built by modules/mono/build_scripts/build_assemblies.py. It does not fetch a different Godot SDK or rebuild the engine. The runner uses real CSharpScript objects and checks native free, managed explicit Dispose, synthetic cleanup notification, nested native cleanup, cross-owner explicit Dispose, signal disconnection and RefCounted native-hold return. An actual completion marker and the strict full engine log are both required. Native predelete must not emit the Object association-mutation error.

The baseline failure was observed in the real Mono editor Linux PCK export, not this fixture; this new fixture has not yet been compiled/run in the author's environment because dotnet is unavailable. The Mono CI workflow runs it after its existing managed build. Normal export regression coverage remains unchanged.
