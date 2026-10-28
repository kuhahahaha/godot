# E22 core texture snapshot exports — 2026-10-29

This slice closes the D/core source-export gap for the adopted E22 texture
adapter. Godot now registers real `ange_texture_snapshot_plan_v1` and
`ange_texture_snapshot_v1` exports for retained `CurveTexture` and
`GradientTexture1D` objects. The functions read the original child resource,
carry separate monotonic wrapper/child content revisions, validate the exact
source tuple and plan, and emit the actual curve/gradient pixel representation.
They do not mint an owner, claim, permit, E20 transaction, GPU completion or
Ready state.

## TDD and validation

- RED build rc2 after the test harness was corrected: the two texture exports
  were undeclared.
- Debug GREEN build rc0; focused test is 1 case / 32 assertions; the full
  `[ResourceSnapshot]*` regression is 14 cases / 184 assertions.
- The native regressions remain green: device-exit 133/2619, Graph 8/109,
  Qualification 20/346, and K03/P2 22/350, all rc0.
- macOS arm64 `template_release dev_build=no tests=no` builds rc0 and
  `--version` returns rc0.
- The actual `use_ubsan=yes` editor build produces
  `bin/godot.macos.editor.dev.arm64.san` with rc0; its focused and full
  ResourceSnapshot runs pass 1/32 and 14/184, rc0. Earlier `sanitize=ubsan`
  invocations were not used as sanitizer evidence.

## Contract boundaries reviewed

Source tuple equality is fieldwise, so it never relies on undefined struct
padding. `ObjectID` values are observational wrapper/child identities only;
revisions come from dedicated saturating content-revision fields on the texture
wrappers and their Curve/Gradient children. Missing children return typed
`ANGE_TEXTURE_INVALID_SOURCE_V1`; they do not produce synthetic zero pixels.
Plan and snapshot validate width/format limits, exact source/revision, output
and scratch capacity/alignment, and disjoint metadata/output/scratch spans.
Curve sampling follows the existing `CurveTexture::_update` indexing and RGB /
Red modes. Gradient sampling follows `GradientTexture1D::_update` indexing and
HDR/LDR output formats.

No E20/B owner transaction, native/GPU qualification, Ready or G1 result is
claimed. The remaining E22 runtime gate is the original owner transaction and
live adapter caller, not this core export implementation.

## Finite-source safety follow-up

The snapshot path now validates finite, ordered Curve points and finite
Gradient points before calling the engine's sampling accessors. This prevents
malformed source data from reaching sorting/sampling code. The follow-up Debug, Release and actual `use_ubsan=yes` builds are rc0; focused and full
ResourceSnapshot runs remain 1/32 and 14/184, rc0. The recorded artifacts carry
the pre-commit build identity `d7d16972ad`; their exact source hashes are
pinned in `SOURCE_MANIFEST.json`, while the current source head is recorded
there separately.
