# Independent ordinary editor --script SceneTree review

2026-11-01. Reviewer `/root/astra_multirange_review`. Reviewed the one-line `FIX_CANDIDATE.patch`, current production source, behavior probe/runner and retained three-case receipt. No production file was edited; no engine, build, GPU or test process was started. This review file is the sole write.

## Conclusion

No definite must-fix found. `if (editor && script.is_empty())` correctly restricts the default editor SceneTree allocation to the branch that actually uses that default. A script-selected SceneTree becomes the first SceneTree and owns the singleton instead of overwriting the only local pointer to a hidden first tree. The candidate is a source-reviewed repair, not yet a rebuilt GREEN receipt.

## Causal source chain

At `main/main.cpp:4543`, the old editor branch allocates a SceneTree before script selection. At lines 4551–4572, a valid `--script` independently instantiates its native MainLoop base and assigns the resulting script loop over that pointer without deleting the editor tree. `SceneTree::SceneTree` at `scene/main/scene_tree.cpp:2050` takes the singleton only if it is null; the second selected tree therefore does not become singleton. Only the selected loop is registered with OS at main.cpp:4619 and iterated thereafter. The macOS loop setter/deleter own only that selected pointer.

`Node::queue_free` at `scene/main/node.cpp:3461` uses the node's own tree while attached and the global SceneTree singleton while detached. The hidden first tree does not process/finalize, so its delete queue never drains. The selected tree flushes its own queue during physics/process and finalize. Destroying the selected second tree does not clear the hidden first tree's singleton because the destructor clears only `singleton == this`.

The one-line condition prevents this unwanted first allocation. It does not rewrite SceneTree singleton ownership, force-delete a tree after a replacement is already constructed, or change legitimate explicit multiple-SceneTree behavior.

## Adjacent paths

| Path | Source review |
| --- | --- |
| Editor with no script | Condition remains true; default SceneTree, editor-forced loop selection and later EditorNode initialization remain unchanged. |
| Editor with SceneTree-derived script | Script supplies the sole selected SceneTree. Later `sml` succeeds and the existing editor initialization attaches EditorNode to that same selected tree. |
| Editor with plain MainLoop-derived script | The script still supplies the selected loop; `sml` is null before and after this patch, so the existing SceneTree/EditorNode block is skipped. The unused global SceneTree is removed. Detached Node.queue_free without any user-created tree now reports no SceneTree instead of queuing forever on the leaked tree; plain MainLoop scripts must manage their own node/tree lifetime. |
| `--script --check-only` | Return remains immediately after script load/validation, before loop selection. The unused editor tree is no longer allocated on this early exit. |
| Missing/invalid/uninstantiable script | Existing error/result paths remain before OS loop selection; an instantiated non-MainLoop object is still explicitly deleted. The unused editor tree is no longer leaked. |
| Runtime script without editor | Default-editor condition was already false, so script selection is unchanged. |
| `--main-loop` / project main-loop type with editor but no script | Existing editor SceneTree precedence remains intact. Non-editor global-class loop selection is unchanged. |
| Override disabled / script cleared by configuration | The condition uses the final effective `script` after override handling, preserving the ordinary no-script editor path. |

These are source conclusions. The retained behavior receipt does not execute plain MainLoop, check-only, invalid-script or no-script editor cases after the fix.

## Behavioral RED and controls

`behavior/results/20261101-113415/RESULTS.json` is a meaningful ordinary headless behavioral RED against pinned executable `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58`:

- editor + detached: the child exits the selected tree once, survives two selected-tree process frames, and has no predelete notification; process exits 42 with the exact failure result.
- runtime + detached: child becomes invalid, with one exit and one predelete; process exits zero.
- editor + attached: child becomes invalid, with one exit and one predelete; process exits zero, because attached queue_free uses `data.tree`.

The reviewer independently recomputed the pinned host, controlled-build metadata, script, project and all three log hashes and matched the receipt. Current main.cpp still contains the original allocation branch exactly once; the candidate has not been applied at this review snapshot.

The test does not mistake tree exit for deletion: it independently observes `NOTIFICATION_PREDELETE` and `is_instance_valid`. It awaits `self.process_frame`, not a possibly wrong singleton's frame signal. Manual `free()` occurs only after the RED result is recorded, and the runner verifies that ordering; it cannot turn RED into the exact GREEN result. Parse/script errors, unexpected enrollment/detach errors and timeout are excluded by the runner. The leak count is diagnostic, not an acceptance condition: both old editor cases report 209 leaked ObjectDB instances, while only the detached behavior is RED. This separation is appropriate.

The runner's `--expect-fixed` requires all three actual-free results, so retained baseline GREEN controls are not presented as evidence that the candidate itself passed. Future passing output can establish the specific deletion behavior, but not absence of every editor leak; broad shutdown/log cleanliness is outside this test's contract.

## Unverified boundaries

A fresh binary containing this one-line repair must run the three-case suite with `--expect-fixed`. Adjacent ordinary headless smoke checks should include a plain MainLoop script that exits, valid and invalid `--check-only`, a script not inheriting MainLoop, and a bounded no-script editor launch. This reviewer has not run any of them. GUI/display-backend behavior, project-manager/export/import combinations, extension SceneReady/close semantics and all native/GPU/ANGE product acceptance are also unverified here. No evidence claim is made beyond the retained ordinary headless baseline and inspected source.

## Fingerprints

| File relative to repository | SHA-256 |
| --- | --- |
| `work/native-exit-editor-audit-20261101/FIX_CANDIDATE.patch` | `f18a6e75d73a3f9b0e8496ac1bbdf1ce17c612ca5e561e07be96f9d51d310be0` |
| `main/main.cpp` | `ba7027f3158f1faf5e3e69cb0702156b49642adff97f6e4c045ba5aa26057ce1` |
| `scene/main/scene_tree.cpp` | `eed6dbff323344efff19f2de6763705aff062a76e00fd7cbe42a4ee849b7e5a4` |
| `scene/main/node.cpp` | `0db10a968bd027fec03f6b78ddf793660001f61eacb369cf4cf2cf6dd48f2b80` |
| `platform/macos/os_macos.mm` | `47939c0918fe2dd391ce2d1a7aed9b6d33ea0dddf839bfc393412b7f213a1a06` |
| `work/native-exit-editor-audit-20261101/behavior/probe/queue_free.gd` | `a9d59531b3f53c08b7a0ee3d245132189b09390811fc78d7640a694c059b23d3` |
| `work/native-exit-editor-audit-20261101/behavior/run.py` | `cda994d5c4f753ea329ca92f8c0aae5d8e5df18a5acd08631b63dbc5f2310562` |
| `work/native-exit-editor-audit-20261101/behavior/results/20261101-113415/RESULTS.json` | `287bfa87e346266b78393857d4ef7036c54ed95876f2ab7ed83956912a322990` |
