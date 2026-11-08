# Generation source surface borrow

Implemented bounded source custody for complete INSTANCE_192 generation directories.
The original metadata SurfaceCell table owns each reverse consumer edge, and the
original generation pin is borrowed through Registry. No allocation, new issuer,
pass/view authority, or ABI entry is introduced by acquire. Generation-level cells
use the existing slot field's UINT32_MAX sentinel; legacy slot returns remain separate.

Acquisition validates the render thread, complete retained source/material selection,
ordinal, original generation root and available cell/pin budget before publishing a
selection. Move transfers the reverse edge. Return validates the exact cell, serial,
generation, source pointer, mesh/native ordinal and material identity before unpinning.
Withdraw blocks new borrows. Existing borrows remain returnable, and generation
retirement refuses with ERR_BUSY before releasing source storage while custody lives.
The count is a retained directory diagnostic; it does not promise that pin capacity or
request admission will allow acquisition. Classification and priority remain unclassified
defaults and are not evidence of renderer pass eligibility.

## Validation

Base: 5b4cd23690. macOS arm64 editor, CPU/dummy doctest only.

Build (exit 0, 19.61 seconds):

```sh
scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2
```

Binary SHA-256: ea472814be7d5b216f10c27f1ac3b2b5ef543be10779b9a0342cb865818d3700.

| Filter | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| `*[GenerationSurfaceSource]*` | 2 | 740 | PASS |
| `*[GenerationSelection]*,*[OutputSceneMetadata]*,*[AliasReadbackIntegration]*,*[AliasReadbackMapFailure]*,*[NativeSceneReleaseMaintenanceBudget]*,*[ExternalOutputOwner]*` | 16 | 4581 | PASS |
| `*[ExternalOutput]*,*[GenerationCreate]*,*[GenerationMetadataLayout]*,*[GenerationSelectionLayout]*,*[EngineGuardLifecycle]*` | 6 | 449 | PASS |

All runs used `bin/godot.macos.editor.arm64 --headless --test --test-case='<filter>'`
and exited 0 with zero failed assertions. Raw local logs: `/tmp/b4-surface-borrow-final-build.log`,
`/tmp/b4-surface-borrow-final-focused.txt`, `/tmp/b4-surface-borrow-final-neighbors.txt`,
`/tmp/b4-surface-borrow-final-legacy.txt`.

The prior stub RED produced four expected failures (0 count, unavailable acquisition,
empty selection, absent return). Initial implementation also exposed insufficient
fixture pin quota; the final fixture independently exhausts the original pin quota
and the seven-cell metadata pool, proves unchanged empty output, then succeeds after
return. Additional coverage includes invalid ordinal, occupied output, move, foreign
thread refusal, withdraw refusal and an actual bounded OBSERVE that preserves all
source/material/mesh counts while a borrow survives. RT return then allows the existing
bounded retirement sequence to finish. Independent Sol review found no blocking issue;
the final static Callable callback adjustment received scoped re-review.

Source-field tampering and serial non-consumption are source-reviewed rather than
directly asserted in these new tests. No GPU, renderer pass opener, public view/resolve,
visible Sprite draw, no-follow-up-draw closure, or FullBirth acceptance is claimed.
The long-running goal remains active.
