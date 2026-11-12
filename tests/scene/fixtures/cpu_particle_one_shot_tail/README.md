# CPU one-shot tail engine regression

Run from the repository root:

```sh
python3 tests/scene/fixtures/cpu_particle_one_shot_tail/run.py --godot /absolute/path/to/godot --output /absolute/path/to/receipt.json
```

The runner copies the two project files into a temporary project and writes `.godot/global_script_class_cache.cfg` with `list=[]`. It records the exact command, binary SHA-256/mtime/size, project hashes, engine exit, unique result marker, raw stdout/stderr and timeout/error state. JSON output has companion `.stdout.log` and `.stderr.log` files. It exits zero only if there is exactly one valid marker, its failures array is empty, the engine exits zero, and no ERROR diagnostic, RID/ObjectDB leak diagnostic, timeout or binary mutation occurs. Nonempty failures require engine exit 1 and make the runner exit 1. Missing/duplicate markers and mismatched exits also fail. `--timeout` defaults to 30 seconds. Stderr is always retained; the cache workaround produced empty stderr in both recorded final runs.

## Coverage

Both CPU particle dimensions use lifetime=.875, amount=8, fixed_fps=4, one_shot=true, fixed seed, zero gravity/randomness/explosiveness/lifetime randomness and fractional delta. Natural advance is paused with speed_scale=0; public request_particles_process(1.0) temporarily uses speed 1 and executes four .25 simulation calls. Residual processing drains without restarting emission. Public restart(true) is asserted immediately through `p.emitting == true` before the next `request_particles_process` call, then repeats the cycle. The second cycle proves a fresh tail after the buffer was cleared plus immediate restart activation; it does not independently prove full restart reset/time/seed behavior. Finished counts must reach 1 after the first drain and 2 after restart/drain.

3D additionally reads the public CPU-populated MultiMesh buffer using get_base()/RenderingServer.multimesh_get_buffer. After four steps and after restart, it requires size160 and nonzero transform for index7, the first-cycle tail particle scheduled at .765625. After both drains it requires size160 and no active transforms. Final CPU data is published with a deferred force_draw/force_sync from finished before the next inactive update disconnects frame_pre_draw; otherwise the public buffer can retain the previous frame even though finished has fired. No production hooks or private-state access are used.

2D checks only public lifecycle/finished/restart state; its birth count is not observed because this fixture has no public particle-buffer RID for 2D. These are headless real-engine CPU particle/public-buffer checks, not GPU simulation, rendering, or pixel-output acceptance. This fixture does not prove absence of all duplicate births, exact initializer seeds, or fractional motion; those are covered by separate source controls rather than inferred from emitting=false.

## Recorded RED/GREEN

Commands actually executed:

```sh
python3 tests/scene/fixtures/cpu_particle_one_shot_tail/run.py --godot /Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/particle-current-engine-build-20261111/shader-diagnostic/godot.before.arm64 --output tests/scene/fixtures/cpu_particle_one_shot_tail/red-strict.json
python3 tests/scene/fixtures/cpu_particle_one_shot_tail/run.py --godot /Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/particle-current-engine-build-20261111/source/bin/godot.macos.template_debug.arm64 --output tests/scene/fixtures/cpu_particle_one_shot_tail/green-strict.json
```

- RED: engine exit1, runner exit1. Index7 is absent initially and after restart; observed active slots [2,3,4,5,6]. Both restart rows record immediate public `emitting=true`. Both drain buffers are empty with size160, finished counts1/2. Binary hash is in red-strict.json.
- GREEN: engine exit0, runner exit0, empty failures. Index7 is present initially and after restart; both restart rows record immediate public `emitting=true`; both drain buffers are empty with size160, finished counts1/2. Binary hash is in green-strict.json.

Earlier red.json/green.json and their raw logs preserve the first diagnostic attempt before final-buffer publication was added; both failed the then-stale drain-buffer check. These are local exploratory artifacts, absent from the committed fixture, and are not required reproduction inputs. They are not the final acceptance receipts. Original work-directory evidence was not changed. No engine build, source/snapshot change or commit was performed for this fixture task.

The source-slice controls can be rerun independently of an engine binary:

```sh
python3 tests/scene/fixtures/cpu_particle_one_shot_tail/run_source.py \
  --source-root /absolute/path/to/godot \
  --baseline-ref afafb0968c \
  --output /absolute/path/to/source-slice-receipt.json
```

This resolves the baseline through Git, reads both current CPU particle sources, extracts the production scheduling/seed/age/finished statements, compiles every slice before running it, and requires 81 checks per variant. It runs 18 variants: baseline RED, current GREEN, and seven targeted mutation controls per dimension. Generated files are temporary; the JSON receipt retains source hashes, compiler commands, outputs, and the exact resolved baseline commit. The controls are source-slice evidence only and must not be reported as engine, GPU, renderer, or pixel acceptance.

## Runner calibration correction

The initial Python fake-executable controls all timed out before emitting any marker. They did **not** independently verify duplicate-marker or exit-mismatch handling. That original evidence is retained locally as `runner-controls-diagnostic.json`; this exploratory artifact is absent from the committed fixture and is not a required reproduction input or acceptance evidence.

`python3 tests/scene/fixtures/cpu_particle_one_shot_tail/check_runner_controls.py` now runs immediate `/bin/sh` scripts with 5-second limits for normal controls and an independent 0.2-second sleep timeout. `runner-controls.json` stores the full script text, runner command, runner exit/stdout/stderr, marker count and full engine receipt. Persistent scripts and their original stdout/stderr/receipt files are in `runner-controls/`.

Verified individually: missing marker (count0, engine0, no timeout); duplicate marker (count2, engine0, no timeout); exit mismatch (one empty-failures marker, engine1, no timeout); dedicated timeout; ERROR diagnostic with clean marker/engine0; RID leak diagnostic with clean marker/engine0; clean marker/engine0 accepted. Normal negative controls fail specifically for their intended condition. The runner now rejects ERROR diagnostics on either stream and RID/ObjectDB leak diagnostics even when warnings. Other warnings are retained but do not independently fail it. This is a diagnostic matcher, not a guarantee of detecting every possible engine warning or leak wording.

After this correction and the immediate restart assertion, both real binaries were rerun with the strict runner: `red-strict.json` engine1/runner1 (two missing-tail failures) and `green-strict.json` engine0/runner0, both stderr empty and both dimensions recording immediate restart activation. The local `red-final.json`/`green-final.json` receipts are historical exploratory artifacts, absent from the committed fixture and unnecessary for reproduction; strict receipts are authoritative for current runner behavior.
