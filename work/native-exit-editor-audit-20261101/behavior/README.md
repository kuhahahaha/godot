# Selected SceneTree queue ownership behavior control

No production/source startup change or scons build was performed by this worker. The minimal one-line proposed fix remains `../FIX_CANDIDATE.patch`; main coordination owns its application and build after the current builder finishes.

## Exact behavioral RED and controls

Old Store-capable pinned host SHA `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58` produced:

| Case | Trigger | After two actual `self.process_frame` signals | Exit |
| --- | --- | --- | ---: |
| editor_detached | add child to self.root, confirm child.get_tree()==self, remove child, queue_free | child still valid, tree_exited=1, predelete=0 | 42 |
| runtime_detached | same detached trigger, without --editor | child invalid, tree_exited=1, predelete=1 | 0 |
| editor_attached | add child to self.root then queue_free while still attached | child invalid, tree_exited=1, predelete=1 | 0 |

Source matters: `Node::queue_free` uses the attached node's own `data.tree`; therefore the attached editor neighbor must pass even on the old engine. The actual stale-singleton defect requires the detached branch, matching EditorNode's detached dialog cleanup. No behavior failure is inferred merely from ObjectDB's 209-instance leak summary.

The script records `QUEUE_FREE_RESULT` before diagnostic cleanup. RED requires the exact valid/predelete0/exited1 state after exactly two signals from the actual selected script tree, and a bounded exit42. The failed probe then explicitly frees only its own child so it does not add a Node leak. Its later `QUEUE_FREE_PREDELETE` marker follows `QUEUE_FREE_MANUAL_CLEANUP` and cannot be mistaken for successful queue draining.

All three processes completed without timeout or parse/script errors. Evidence: `results/20261101-113415/RESULTS.json` and its three raw logs. It separately records remaining ordinary editor leak counts; rc0 on the attached control is not called a clean editor exit.

## Reproducible commands

The bounded runner validates the executable and controlled-build hashes from a pin JSON, runs exactly these three cases and checks exact behavior markers:

```sh
# Old host: require the precise detached editor RED and both normal neighbors.
python3 work/native-exit-editor-audit-20261101/behavior/run.py \
  --pin work/physics-wrapper-finish-recovery-20261101/PINNED_HOST.json

# New one-line-repaired source-matched pinned host: require all three actual-free GREENs.
python3 work/native-exit-editor-audit-20261101/behavior/run.py \
  --pin /absolute/path/to/new/PINNED_HOST.json --expect-fixed
```

Equivalent per-case process commands, with `<host>` replaced by that exact pinned executable:

```sh
<host> --headless --editor --path work/native-exit-editor-audit-20261101/behavior/probe --script work/native-exit-editor-audit-20261101/behavior/probe/queue_free.gd -- detached
<host> --headless --path work/native-exit-editor-audit-20261101/behavior/probe --script work/native-exit-editor-audit-20261101/behavior/probe/queue_free.gd -- detached
<host> --headless --editor --path work/native-exit-editor-audit-20261101/behavior/probe --script work/native-exit-editor-audit-20261101/behavior/probe/queue_free.gd -- attached
```

The runner resolves all paths absolutely; absolute script paths avoid command-line path interpretation differences in manual invocations.

## Original capability probe as separate clean-exit observation

The original copied capability input and old/new comparison are already preserved at `../baseline` and `../COMPARISON.json`. Both old pinned hosts reported all capability fields true with exactly the same 209 ObjectDB / Canvas6 / CanvasItem36 etc. leaks. The export-plugin local is nulled and is not among leaked classes.

For the repaired host, this prepared runner copies that exact baseline again into a new work result directory; it never rewrites or reruns the original `/tmp` directory. `--expect-clean` adds explicit zero-resource-leak acceptance independently of capability rc0:

```sh
python3 work/native-exit-editor-audit-20261101/behavior/run_capability.py \
  --pin /absolute/path/to/new/PINNED_HOST.json --expect-clean
```

This prepared repaired-host command has not been run by this worker.

## One-line scope and adjacent startup entrypoints

The proposed condition `if (editor && script.is_empty())` prevents constructing an unwanted editor placeholder before an explicit script. It leaves ordinary editor startup (including a project-defined main_loop_type override) on its existing forced SceneTree route, and leaves noneditor script startup unchanged.

The following source paths get the correct ownership behavior without changing their existing script decisions:

- `--editor --script ... --check-only`: returns after script validity checking before instantiation; no placeholder tree should have been allocated.
- Explicit script load/parse/instantiation failure: the existing failure result remains, with no preallocated editor tree to strand.
- Explicit `extends MainLoop` script: the selected custom MainLoop remains the only loop; no hidden unused SceneTree/singleton is created. It is not a SceneTree and must not depend on an accidental hidden tree to own detached Nodes.

Prepared narrow manual commands for the repaired host (not executed by this worker):

```sh
<host> --headless --editor --check-only --path <probe-absolute-dir> --script <probe-absolute-dir>/queue_free.gd
<host> --headless --editor --path <probe-absolute-dir> --script <probe-absolute-dir>/does_not_exist.gd
<host> --headless --editor --path <probe-absolute-dir> --script <probe-absolute-dir>/custom_main_loop.gd
```

Check-only should retain validity success; missing script should retain a nonzero explicit load error; the custom MainLoop should print its initialization marker and exit normally. In each case separately inspect exit resource diagnostics. These commands are proposed neighbors, not claimed results. Do not delete the first tree only after constructing the replacement: its destructor clears the singleton and the already constructed second tree does not re-register.

## Prepared GREEN neighbor runner

`run_neighbors.py` prepares the exact adjacent positive controls requested after review. It does not require changes to Main startup beyond the one-line candidate:

```sh
python3 work/native-exit-editor-audit-20261101/behavior/run_neighbors.py \
  --pin /absolute/path/to/new/PINNED_HOST.json --capability
```

It runs three bounded ordinary headless subprocesses: the actual selected SceneTree queue script with `--check-only` (validity success, no script execution), the editor with no `--script` and a normal `--quit-after 8`, and an explicit plain MainLoop script. Each checks rc0 and exit-resource diagnostics separately. The optional final `--capability` reuses the copied original capability input through `run_capability.py --expect-clean`.

The plain MainLoop observation checks `Engine.get_main_loop()==self`, that self is not SceneTree, and in the tested debug editor build that the native orphan-Node inventory contains zero Window and zero EditorNode objects; it also requires no EditorNode first-scan startup and no exit-leaked SceneTree. This combines actual selected-loop/orphan inventory/exit evidence with the source's `if (sml)` EditorNode construction gate, rather than assuming rc0 proves no placeholder was created. No engine-side instrumentation is added.

This new custom MainLoop and neighbor runner have only been prepared; the repaired-host commands have not been executed by this worker. Python runners pass byte compilation. The earlier three-case behavioral RED logs and queue script remain unchanged.

The prepared custom MainLoop passed a parse-only `--headless --check-only` run on the pinned old edf host (`CUSTOM_MAIN_LOOP_SYNTAX.json`, rc0). An initial fixture draft tried `OS.set_exit_code`, which is a C++ API but is not exposed to GDScript in this fork; the parser rejected it. That draft was corrected by removing the call, and its failed parse evidence is retained as `CUSTOM_MAIN_LOOP_SYNTAX.red.json` / `custom-main-loop-syntax.red.log`. Custom-loop acceptance depends on the structured observation plus the Python runner result, never solely on process rc0. No repaired-host neighbor or capability execution is claimed by this parse-only check.
