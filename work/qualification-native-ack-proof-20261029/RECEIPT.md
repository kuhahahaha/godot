# Native-unit retirement acknowledgement association — 2026-10-29

Scope: original Godot native-unit retire release; source parent `c830d92303ce91f77a7d6710957d5d90376833aa`. This is an independently testable native ownership prerequisite, not T1 texture/sampler/set implementation or complete ANGE qualification.

The original release gate relied on report masks and return status without comparing each reported native return to its actual original shader/pipeline physical cell. It could acknowledge an internally inconsistent report in a controlled cell fixture. No actual production report corruption or erroneous ANGE disposal was observed. The strengthened gate retains the original Registry and physical cells and checks each known object's original bridge/child, RID, serial, closed use, queued frame/epoch and exact nonzero reserved/driver-return event against its backing and report. It requires the original dependency serial, valid report, no error/possible mask and distinct object events. Current actual creation produces shader-only `known=1` or shader+pipeline `known=3`; a self-consistent pipeline-only `known=2` is rejected. Shader-only requires zero wire fields for its uncreated pipeline, while its unused internal cell may retain `UINT32_MAX` frame slot. Two real objects may return in different frame slots and epochs. No extra GPU completion source is created.

This is a D acknowledgement defence. ANGE currently may consume a Returned report before calling D release, so this gate alone does not prove end-to-end prevention of an invalid report. Publication-time and consumer-chain evidence remain separate. Driver-free event means the existing native owner call returned, not OS/GPU memory reclamation.

## Verification

The local `scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes` build returned 0; `godot-native-ack-proof-final-build.log` contains its output. The linker warned about the installed MoltenVK object's macOS 12 minimum versus target 11. Editor SHA-256 `4ca127bc65b5ebd0d86db8ea3f0bf21f4164ebe93137d4c80155de2793dc42c2`.

`godot-native-ack-proof-red2-run.log` shows the original mask-only release accepted inconsistent controlled cells; subsequent failures cascade after the first ack. `godot-native-ack-pipeline-red-run.log` separately shows the initial strengthened implementation accepting an otherwise self-consistent pipeline-only unit; this was fixed before the green run. Tests restore a valid original-shaped fixture on every mutation and exercise 30 distinct shader/pipeline, report, mask, frame, event and identity perturbations for each shader-only/two-object lane; cross-frame and synthetic Returned/MAX acknowledgements remain accepted. These are controlled cells, not real GPU error injection.

After repair, `bin/godot.macos.editor.dev.arm64 --headless --test --test-case=<pattern>` returned 0:

| Pattern | Cases | Assertions |
| --- | ---: | ---: |
| `[Qualification]*` | 44 | 900 |
| `[QualificationRt]*` | 4 | 378 |
| `[RenderingDevice][Qualification]*` | 7 | 46 |
| `[K03][P2]*` | 22 | 350 |
| `*device_exit*` | 133 | 2619 |
| `*FrameWait*` | 17 | 301 |

`python3 work/qualification-native-ack-proof-20261029/run_metal_controls.py` returned 0. Existing positive and cancellation-prefix substrate controls both passed all 17 checks on Metal 4.0 / Forward Mobile / Apple M3 Pro, including real two-unit and eight-buffer return events, bridge close and finalizer/manager close. Inputs were hashed before and after and unchanged in `metal-results.json`. These use previously generated source/containers (generation editor SHA `720900340909ef176290cb20da1f28b903bd1918e1e92ef5e5e54bbb0e0ba197`) with this new editor; no new matched source/compiler tuple or real failure injection is claimed. Shader-only prefix is tested with controlled cells, not a real shader creation failure.

Read-only reviewer `/root/t1_independent_review` checked original contracts, original cell publication, lock order, shader-only and cross-frame cases, all 30 perturbations and the pipeline-only negative. No explicit new P1/P2 was identified. The reviewer did not edit or run the build.

The non-maintenance Unknown intermediate state/reason issue and raw maintenance-error provenance remain open, as do T1's actual texture upload, sparse method sets, method record and cross-side integration. The earlier maintenance-failure receipt records the pre-change acknowledgement gap as historical evidence; this receipt supersedes that single gap, not its other limitations. Long goal remains active.
