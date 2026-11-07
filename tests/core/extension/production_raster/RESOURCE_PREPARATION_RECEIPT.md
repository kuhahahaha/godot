# Independent raster resource preparation verification

Status: implementation, independent source/test review, complete editor build and CPU runtime verification passed on 2026-11-06. This continues the Production raster and Sprite LE192 goal and does not establish visible drawing or real-GPU acceptance.

The new prepare/query procedures operate on the original accepted Production request, FIFO control, retained shader bytes and four physical return cells. Caller serials must be zero; Core assigns one owner group and four distinct physical/retire identities. Construction is depth → shader → framebuffer → pipeline. Exact repeated input resumes/reads the same attempt; changed input rejects. A returned failure never retries native creation.

All four RID/free-event reservations precede allocation. Failure retains the created prefix and retires it through bounded original OBSERVE maintenance. Framebuffer/pipeline dependents return before depth/shader; shader readiness is checked without blocking. Shared same-RD disposal service authenticates exact original physical-cell pointers and preserves each resource's original receipt owner. Preparation and retirement only set native history, allowing a later exact no-append fact while native return remains outstanding.

Independent read-only source review checked immutable control replay, serial/RID/event reservation, failure cleanup, ownership, shared fresh/resumed maintenance, actual release dependencies, narrow framebuffer permissions and per-factory original admission/device/finalization checks. No blocking source issue remained. Public cancel/close retain their existing execution gate; the earlier hypothesis that they could succeed during factory execution was withdrawn after source verification. Original quiesce/device loss remain relevant recheck boundaries.

Focused fixture has seven subcases: success, four constructor refusal stages, and true begin_quiesce calls inside depth/shader factories. It uses the installed public prepare/query procedures, real original control FIFO, RD resource owners and mock driver. It checks exact same-key replay, changed-input rejection, retained prefix/physical identities, native-versus-append history and one-receipt/one-pass retirement with final release. Additional assertions reject nonzero caller serials, insufficient return/payload capacity before admission, prove a normal input can follow payload rejection, verify executing public cancel returns Busy, and hold shader retirement across two not-ready observations before readiness. Runtime counts follow after execution.

Commands:

```sh
python3 -c 'from core.extension import make_interface_header as m; m.run(["core/extension/gdextension_interface.gen.h"], ["core/extension/gdextension_interface.json"], {})'
scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3
bin/godot.macos.editor.arm64 --headless --test --list-test-cases --test-case='*RasterPrepare*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*RasterPrepare*'
bin/godot.macos.editor.arm64 --headless --test --test-case='*ProductionRequest*'
bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gpu_particles.cpp'
```

Initial build `/tmp/b4-raster-prepare-build.log` failed at test compilation: physical-cell private fields lacked test access and two doctest OR expressions required explicit parentheses. The fixture was repaired with TESTS_ENABLED-only friendship and parenthesized expressions, retaining the identity/dependency/free-event assertions. Replacement build: `/tmp/b4-raster-prepare-build-v2.log`.

A separate generated-header C++ layout check passed: prepare size 48/alignment 8, report size 120/alignment 8 and every public field offset. This proves ABI layout only.

Remaining acceptance boundaries: successful original checked begin/end join, procedural geometry, shader output/readback and real GPU submission/completion. No mock-driver result is reported as any of those. The subsequent Scene sqrt queue uses only the combined candidate at `/Users/macbookair/Documents/Codex/2026-10-15/xu-y/scene-sqrt-qualification-20261107/combined/HANDOFF.md` (patch SHA-256 f6f5cf13229c5890c4558fe2f6f1010716f6157cef73681fff26585214198887). Its synthetic expected rows are not GPU measurements.

Test review clarified the framebuffer branch: its constructor refusal is a real `render_pass_create` refusal. As this fixture does not enter a draw list, framebuffer storage-map versions remain empty; its return is the original empty-cache release event, not a driver framebuffer-free callback. Pipeline and shader do execute their CPU driver-free callbacks. Remaining component gaps include foreign/same-RD neighbor disposal prefixes and mid-factory device-loss/finalization, although the source guards were reviewed.

Runtime diagnosis and repair: the initial fixture frame epoch was zero, so original free admission correctly refused enqueue. Measured diagnostics showed retiring resources with zero enqueue/free events and no native obligations. The fixture now establishes the same valid empty CPU frame epoch as existing native fixtures; no free guard, service budget or retirement expectation was relaxed. A subsequent run passed all retirement assertions but found the test's quiesce return expectation wrong: a Core factory has no DSO callback/binding holder, so original begin_quiesce returns OK while sealing admission. The expectation was corrected to exact OK; public cancel still must return Busy.

Verified results (all commands exited 0):

| Check | Cases | Assertions |
| --- | --- | --- |
| RasterPrepare (1 listed case, 7 subcases) | 1/1 | 6,959/6,959 |
| All ProductionRequest | 61/61 | 45,287/45,287 |
| Production native observation | 22/22 | 17,617/17,617 |
| GPU particle CPU tests | 18/18 | 90/90 |

The additional observation command was `bin/godot.macos.editor.arm64 --headless --test --source-file='*test_production_native_observe.cpp'`. Full build completed in 7m52s, followed by targeted recompiles for the added/repaired fixture. The linker emitted the existing MoltenVK macOS target warning. Full ProductionRequest emitted exactly three expected negative pipeline-driver diagnostics; focused prepare, native-observe and particle runs emitted no engine errors.

Exact fixed source/editor/controlled-build identities and verification logs are recorded in RESOURCE_PREPARATION_RECEIPT.json after fixed-artifact export. No successful draw, GPU completion or driver framebuffer-free claim follows from these CPU results.

Fixed implementation commit: `3ee60f924d83f1e190c0dced5447a0aae63a027f`. A post-commit build regenerated the actual controlled-build identity and linked successfully (13.59s). All four groups above passed again on these final bytes, including focused execution from the copied artifact itself. The fixed export is `work/fixed-core-3ee60f924d/`, containing editor, matching controlled receipt, seven-field selection, complete identity snapshot, generated ABI and FIXED_ARTIFACT.json. Future builds use the normal bin path and do not overwrite this export.

Editor: 156,448,920 bytes; SHA-256 `d9f3335d4e1b5693f17ad53560cec977e11abd9f80230273a625a173727216de`. The controlled receipt artifact length/hash and all seven selection fields were compared exactly to the copied editor and exported selection before handoff.
