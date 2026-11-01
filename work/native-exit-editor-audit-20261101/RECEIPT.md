# Ordinary headless editor exit resource audit

2026-11-01. Repository `/Users/macbookair/Documents/Work/godot_ANGE`.

## Result

The main coordinator applied the one-line MainLoop-selection fix and built an immutable source-matched executable (build rc0, 21.02 s, source drift empty). This worker then verified real deferred deletion and clean exit on executable SHA `a0cd958938bebf1ac698728c38e7dee9e5b623a4466fbea48ab9e304038d792f` / controlled-build metadata SHA `e0b5774e7d87feea12e15ea510e85a65ba39bafea71bcaf477cea94f65b8fce8`.

The formerly failing detached-editor child now becomes invalid after two actual selected-tree frames, with exactly one tree exit and predelete; both normal neighbors pass. Valid check-only, ordinary no-script editor, and plain MainLoop startup all pass with no exit leaks. The original copied EditorExportPlugin capability probe remains all true and now reports **Canvas0 / CanvasItem0 / ObjectDB0 / other RID0**, independently of rc0. Invalid check-only and missing-script branches return the exact expected nonzero failure, with diagnostics preserved and no exit leaks; a valid non-MainLoop script passes check-only without instantiation.

The pre-fix defect is concrete engine ownership on `--editor --script`, not a retained export-plugin local. The initial old9e24/newedf comparison reproduced identical 209-instance leaks on both. This worker did not modify production source or run scons; the coordinator owns the applied one-line fix and build. Original `/tmp` evidence/probe was not rewritten or rerun in place. No codesign, packaging, Ribbon, GPU or ANGE owner acceptance is implied.

Final source pins, raw evidence hashes, expected exits and the explicit unexecuted runtime limitation are in `VERIFIED_GREEN.json`. Main/SceneTree/Node/EditorNode match the recorded build cpp/h source window. That window omits Objective-C++ `.mm`; the OS_MacOS source reference hash is separately identified rather than falsely called a matched window entry.

## Original and controlled comparison

Read the original `capability.log`, `capability-result.json`, and `probe/check.gd`/`project.godot`. Original command was the edf3 pinned executable with `--headless --editor --path <probe> --script <probe>/check.gd`; rc=0. The script constructs an `EditorExportPlugin`, checks three editor capabilities, assigns `plugin=null`, awaits a frame, and calls `quit`. It does not construct manual Node/Viewport trees.

`compare.py` copied the entire original probe, including its existing `.godot` baseline, into this work directory. Before each of the two runs it reset the same local probe path from that exact baseline and compared every input file hash. Both commands used the same input path/script and added the same `--verbose` diagnostic argument. Only the pinned executable changed. Each executable was run once, serially; no extra GPU process or original-directory run was used. Both executable and controlled-build hashes were verified before and after each run.

Exact commands, source/input hashes, pinned host records, capability output, raw log hashes and counts are in `COMPARISON.json`. Raw logs are `old9e24.log` and `newedf.log`.

| Metric | old9e24 | newedf |
| --- | ---: | ---: |
| Process exit | 0 | 0 |
| Capability editor/GDScript/export-plugin checks | all true | all true |
| Canvas RIDs leaked | 6 | 6 |
| CanvasItem RIDs leaked | 36 | 36 |
| ObjectDB summary instances leaked | 209 | 209 |
| RendererViewport Viewport allocations | 6 | 6 |
| DummyTexture allocations | 9 | 9 |
| RendererSceneCull Scenario allocations | 1 | 1 |
| TextServerAdvanced shaped-text allocations | 83 | 83 |
| TextServerAdvanced Font allocations | 1 | 1 |

There is no count regression between these two artifacts. `git blame` places the relevant eager editor SceneTree allocation and script replacement lines in long-standing source, not the current Store/empty-domain increment. This audit does not infer behavior of an untested external Godot release.

Verbose ownership evidence in both logs includes one leaked `SceneTree`, one `Window`, five `AcceptDialog` instances, six `World2D` and six `ViewportTexture` objects, and one World3D/multiplayer family. No leaked `EditorExportPlugin` or GDScriptFunctionState was listed. Some verbose `Leaked instance` lines interleave with `Cannot get path of node as it is not in a scene tree` diagnostics emitted by ObjectDB's own verbose formatter. Use the explicit ObjectDB summary count 209, not a naive count of intact verbose lines.

## Exact pre-fix source ownership chain

1. `main/main.cpp:4543-4546`: editor startup eagerly allocates a plain `SceneTree` into local `main_loop`.
2. `main/main.cpp:4551-4572`: explicit `--script` loads the script, instantiates a second `SceneTree` at line4561, attaches the script, then overwrites `main_loop=script_loop` at line4572. The first tree is neither deleted nor handed to OS.
3. `scene/main/scene_tree.cpp:2050-2053`: the constructor sets global `SceneTree::singleton` only if it is null. Therefore the first, now orphaned tree remains the singleton. The actual selected script tree never becomes it.
4. `main/main.cpp:4619`: OS receives only the second tree. `EditorNode` is attached to that selected tree's root at lines4798-4799. The orphaned first tree still owns its own Window/World2D/World3D and multiplayer objects.
5. `editor/editor_node.cpp:8880-8886,8929-8932,9554-9568`: EditorNode constructs five detached/unparent-when-invisible dialogs (`accept`, `save_accept`, `warning`, `load_error_dialog`, `execute_output_dialog`). At exit, `EditorNode::_notification(NOTIFICATION_EXIT_TREE)` queues those dialogs for deletion at lines978-991.
6. `scene/main/node.cpp:3461-3469`: detached nodes with no `data.tree` resolve `queue_free()` through `SceneTree::get_singleton()`. The five detached dialog roots therefore enqueue on the orphaned first tree, not the active script tree.
7. `scene/main/scene_tree.cpp:855-869`: only the active selected tree is finalized and its queue flushed (before and after root deletion). The orphaned first tree's delete queue is never serviced. `platform/macos/os_macos.mm:277-284` later deletes only OS's selected loop. Hence the first root viewport plus five detached dialog viewports survive, matching six Canvas/Viewport allocations, and their controls/font/theme/shaped-text resources stay retained.

The unused first SceneTree is a direct source-proven leak. The queue routing explains why five separately constructed detached dialog trees remain even though EditorNode has explicit exit cleanup. It also explains the leaked `SceneTree`/Window/World3D plus five AcceptDialog/World2D families in the verbose logs. The probe's export plugin is RefCounted (`editor/export/editor_export_plugin.h:39`); it is explicitly nulled before quit and is absent from the leak list.

## One-line implemented repair

`FIX_CANDIDATE.patch` contains the exact one-line selection correction later applied by the main coordinator:

```cpp
MainLoop *main_loop = nullptr;
if (editor && script.is_empty()) {
    main_loop = memnew(SceneTree);
}
```

This preserves ordinary editor default SceneTree selection while letting an explicit SceneTree script construct the sole original tree/singleton. It also avoids preallocating that editor placeholder on explicit-script load/check/instantiation failure paths.

Do not merely delete the old tree after constructing the script tree: the old tree owns `singleton`; its destructor clears that pointer (`scene_tree.cpp:2259-2260`), and the already constructed second tree does not automatically re-register. Avoid the unwanted tree before creating the replacement, or deliberately redesign the original selection/ownership transaction.

Suggested narrow validation after explicit implementation authorization:

- The copied capability probe on the new source-matched headless editor must still print all true and exit0, with zero Canvas/CanvasItem/ObjectDB/RID leak diagnostics.
- Verbose output must contain no leaked SceneTree or detached AcceptDialog families.
- Check ordinary `--headless --editor` selection still uses a SceneTree and exits cleanly.
- Check explicit `--script` outside editor still runs normally; explicit script load/parse failure and `--check-only` must not leave a placeholder tree.

These checks were subsequently exercised as described in the final evidence below. No extra production edits or scons processes were started by this worker; the coordinator scheduled the one-line incremental build.


## Final behavioral and adjacent verification

- Actual-free suite: `behavior/results/20261101-114300/RESULTS.json` — three processes exit0; editor-detached, runtime-detached and editor-attached all observe node_invalid=true/tree_exited=1/predelete=1 after two actual tree frames. ObjectDB summaries are zero.
- Positive neighbors: `behavior/neighbor-results/20261101-114319/RESULTS.json` — valid check-only does not execute the script; no-script editor initializes/exits cleanly; custom MainLoop is the selected self, is not SceneTree, has no orphan Window/EditorNode, and does not start EditorNode. All exit0 and resource-clean.
- Copied original capability input: `behavior/capability-results/20261101-114322/RESULTS.json` — all four capability fields true, exit0 and Canvas/ObjectDB/other RID leak maps empty.
- Error neighbors: `behavior/error-neighbor-results/20261101-114513/RESULTS.json` — intentional invalid check-only exit1 with exact parse diagnostic; missing script exit1 with exact load diagnostic; valid RefCounted script check-only exit0 without instantiation. All resource-clean, no timeout/crash. Expected failures are preserved in raw logs and classified, not swallowed.

The non-MainLoop runtime cast-failure branch remains source-reviewed, not executed: `OS_MacOS::alert` uses `NSAlert::runModal` even with headless selection. No blocking desktop dialog was launched to manufacture a test result. The existing explicit delete-before-error code was not changed.

## Suggested commit scope

The production delta is **only `main/main.cpp`**. Retain the small deterministic regression fixtures and runners under `behavior/` (queue_free/custom MainLoop projects, safe failure project, run.py/run_neighbors.py/run_error_neighbors.py/run_capability.py), the copied original capability `baseline/check.gd`/`project.godot`, and concise receipts/manifests/independent review. Keep the actual RED/GREEN result JSON and small raw logs as review evidence if repository policy permits.

Do not blanket-add this work directory: immutable engine binaries, `.godot` editor caches, generated `.uid` files, Python caches, large full-source build snapshots and temporary syntax-draft artifacts are runtime evidence/support files, not part of the minimal source fix. The retained fixed-host `PINNED_HOST.json`, build-result and VERIFIED_GREEN source/evidence hashes can reference those artifacts without committing binaries. No commit was performed by this worker.
