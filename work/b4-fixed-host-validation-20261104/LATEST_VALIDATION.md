# Canonical94b fixed-host validation

Finite validation passes on source `94b06dc4876d153492aa0c0e2f3457d2f5afc92e`, fixed attempt-03 host SHA256 `965cb0ef6e137dcf17298d504f404a6f53aaa0421d03d080ef36e9b5b9b24eb6`. Matching BUILD_IDENTITY is copied verbatim in the run. This is a different host/selection from attempt02/159fcc; no old pair or old failed process is relabeled.

Actual run: `runs/20261104T051011Z-c33cc9fc`.

## Engine tests

Discovery returns exactly17 expected names, with no duplicate/skipped name. Four exact execution groups all exit0 with zero assertion failures, exceptions or engine ERROR diagnostics:

| Group | Cases | Successful assertions |
| --- | --- | --- |
| NativeDisposal | 4 | 60 |
| ExternalOutputGuard | 5 | 444 |
| ExternalOutputConfig | 6 | 1103 |
| Spatial inverse model | 2 | 14 |
| Total | 17 | 1621 |

The NativeDisposal cleanup regression now passes with normal shader free and empty dependency-map checks. Its assertions use the engine's real disposal machinery with a fake driver; this is not Metal native physical-disposal proof. The older source2578/host159fcc 55-assertion group still fails its shutdown-error criterion, and its four isolated case logs/EXPECTED snapshots remain unchanged. None of those earlier results were promoted to clean success.

## Actual native Metal mechanisms

Same fixed host, actual Metal4.0/Forward Mobile/Apple M3 Pro(Apple9), main RD render-thread callbacks. No parallel host was launched by this agent.

- Logical unaligned array-length run `array-length/runs/20261104T051011Z-bccca759-metal`: all3 cases pass. Exact20-byte uint storage reports5, exact40-byte prefixed storage reports6, vec4 storage48 reports3, output16 reports4. New sizes28/24/16/16 report7/2/1/4. All input read/write words and prefixed offsets match their frozen literal arrays.
- Aligned run `array-length/runs/20261104T051017Z-50be5eb5-metal`: all3 cases pass with independent literals8/4/3/4 and recreated12/8/2/4.
- Both runs execute A(no-length)-created uniform sets through B(length), recreate buffers/sets with different sizes, and execute B-created sets through A. This tests compatible-set reuse in both directions using genuinely active bindings across two descriptor sets and nonconsecutive binding numbers.
- Guard `guard/metal-results.json`: production12/12 passes with exit0; ignoring source validity causes the precise unchanged source_invalid oracle to fail with exit1 and the exact unsafe nonzero command/status; restored production12/12 passes with exit0. No unexpected engine ERROR/fatal diagnostics occur.

Per-case observed arrays, expected arrays, actual completed readback callback counts, shader/SPIR-V/host hashes, commands, exits and full logs are retained under the run. Every finite result is accepted after actual buffer_get_data_async callbacks; a frame count is not completion. Cleanup only requests public free_rid and does not observe physical retirement.

These are shader/array-length mechanisms through public RD APIs. The native C++ guard wrapper is compiled into the host and its pure layout tests run, but these GPU fixtures construct pipelines directly and do not execute its prepare method. They do not authenticate E09 sources, producer capabilities, renderer draw/adoption, full history/material integration, private dynamic-ring paths or whole B4/ANGE completion.

The additional numeric probe is a separate package at `work/b4-guard-numeric-gpu-probe-20261104/`; it changes none of the12-case guard inputs/results above.
