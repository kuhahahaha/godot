# Fixed-host validation runner preparation

Prepared only. No engine binary or GPU host was executed by this runner; a completed host has not yet been supplied. No engine build was started, no production source was edited and no earlier evidence was changed. Python syntax compilation and argparse help exit0.

The source test inventory contains exactly17 names: NativeDisposal4, ExternalOutputGuard5, ExternalOutputConfig6, inverse-model2. `EXPECTED_TESTS.json` records the exact names and four test-file hashes. Its revision40a7 is the **test-name baseline**, not a restriction on the eventual canonical host commit. The caller must supply the new canonical membership-fix revision explicitly.

Usage after root supplies the immutable host, host SHA and accepted build identity:

```text
python3 work/b4-fixed-host-validation-20261104/run.py \
  --godot /absolute/fixed/host/editor.arm64 \
  --sha256 <root-supplied-host-sha256> \
  --build-identity /absolute/BUILD_IDENTITY.json \
  --source-revision <root-supplied-canonical-40-character-commit> \
  --stage all
```

There is no default executable. The runner verifies the supplied SHA, refuses build-tree bin output (including a symlink resolving there), verifies the four test source hashes and copies BUILD_IDENTITY verbatim. It requires valid JSON but does not guess metadata field names or claim to independently accept the host/source match; root supplies that accepted identity. Every invocation creates a new timestamp/UUID directory.

Engine discovery uses the actual repository doctest XML reporter (`thirdparty/doctest/doctest.h`), not console text guesses. Test filters consist of the full17 exact names; discovery must contain the exact set with no duplicate or skipped name and unskipped17. Four actual execution groups must contain the exact4/5/6/2 names, positive assertion counts, zero failed assertions/cases, true per-case success, no exception and exit0. Unclassified engine ERROR lines fail this runner and remain in raw logs. `--test` is dummy/CPU unit evidence, never GPU evidence. Default stage is engine; runtime/all are explicit choices.

After successful engine jobs in `--stage all`, runtime validation serially runs:

1. New Metal logical/unaligned array-length controls, with strict literal lengths5/6/3/4 and recreated7/2/1/4. Compatible sets are created against the no-length shader and consumed by the length shader, and vice versa.
2. New Metal aligned controls with independent literal expected lengths8/4/3/4 and recreated12/8/2/4.
3. New Metal main-RD guard12-case production baseline, the precise source-validity shader-mutant expected failure, and restored12-case production pass.

These fixtures are new copies under this preparation directory; their eventual inputs/results live only in the new invocation's directory. The old runtime91-file package and committed guard package are not used as output locations. The guard copy now requires an explicit host/SHA/source root, and the unrelated diagnostic-export entry point was removed from this new copy. Readback callbacks remain the completion mechanism; frame counts and main-RD submit/sync are not substituted. Free requests remain requests, not physical retirement. Dynamic/private-driver controls remain unexecuted.

## Independent routine membership-fix review

`membership-review/RESULT.json` records the actual tests/SCsub recursive `*/**/*.cpp` glob, relative to tests/. The renamed `harness.cpp.in` is excluded and the old `harness.cpp` no longer exists. The new template's bytes exactly match the original40a7 template (SHA a1f21e4bf4d4b5099235918e76e67cce4d70b986880f2a1cc689b14593fbff60). run.py changes only the template filename it reads. The README command does not refer to the old filename.

The independent updated fixture command exits0: five production SPIR-V variant compile/validate/layout checks,40 sequential CPU cases, and all3 precise expected-red mutations pass. Raw output is `membership-review/fixture.log`. This verifies that the template remains reconstructible and no longer joins the engine test TU glob; it does not replace a new engine build or GPU check.
