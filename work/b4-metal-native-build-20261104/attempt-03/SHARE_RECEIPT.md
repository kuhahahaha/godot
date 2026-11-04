# B4 finite native mechanism handoff

Source ref: origin/codex/b4-renderer-continuation-20261104 at canonical commit94b06dc4876d153492aa0c0e2f3457d2f5afc92e, repository git@github.com:kuhahahaha/godot.git. Clean source build command: `scons -Q tests=yes platform=macos target=editor -j6`. Seven Metal implementation files are unchanged from40a7; later source changes fix fixture build discovery and test-only shader cleanup.

The completed host, SDK, selected identity and controlled-build sidecar are a distinct pair from the original System4e99/a2. Host binary is retained locally and excluded from Git. BUILD_SOURCE.json, BUILD_RESULT.json, BUILD_IDENTITY.json, sdk/gdextension_interface.gen.h, CoreBuildSelection.schema1.json and host/editor.arm64.controlled_build.json are the lightweight handoff. Source/metadata alone does not transfer runtime owner authority.

Host SHA256: `965cb0ef6e137dcf17298d504f404a6f53aaa0421d03d080ef36e9b5b9b24eb6`. SDK SHA256: `ec25549317a591c3009de34f217bcb835f1d3cc52ba2d8782bde2af15d5dbad7`. Selection SHA256: `6dbeb80c7159785c5edaad0dfb9ae7d16ecb2438bda9f68f8f767144f5586c19`. Sidecar SHA256: `b0a8d4d7bf5e907ddaab9e08070857670c7d2345540defec1b078060e9dc2e0c`.

Selection (actual generated metadata):

```json
{
  "schema": 1,
  "core_stamp": "3f023fca7d5298002cd39f9dc8f1ef76dd2127b29b5344c0fa421c938059aa95",
  "compiler_route": "BuiltinGlslangV1",
  "compiler_stamp": "cd5f0271d623e824d623d222c6e03e451449c9416ac5e13f280b1ecbea10755c",
  "platform": "macos",
  "architecture": "arm64",
  "target_kind": "Executable"
}
```

Finite validation:17 exact discovered/executed tests,1621 assertions, clean exits/diagnostics; actual Metal nonaligned logical lengths and aligned compatible-set/recreation controls; actual Metal guard12→targeted source-validity RED→restored12; independent default-policy numeric probe9/9 including seven exact fault16/zero-command negatives. Raw runs and scopes are in the fixed-host validation and numeric probe packages. The broader258-case suite has all50430 assertions passing but eight ERROR lines whose negative-case baseline attribution is being checked separately; it is not described as clean.

No new C++ wrapper pipeline execution, production E09 source/budget/operation association, actual external renderer/adoption, dynamic ring execution, native physical-free/owner ACK or whole B4/System/U1/U05 acceptance is conferred. Old failure records and original matched System pair remain unchanged.
