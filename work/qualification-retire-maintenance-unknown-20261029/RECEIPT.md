# Qualification retire maintenance failure — 2026-10-29

Scope: Godot original Registry/RenderingDevice storage and native-unit retirement; source parent `caa8a25da3ab3f6e3a0ebd335210eaea409455de`. This is not T1 texture implementation, a production ANGE request, a hardware-failure injection, or full GPU/Ready/G1 acceptance.

A non-BUSY error from original `_qualification_maintenance()` was written to a retire report only *after* a second `qualification_retire_step`. A real native-call poll error can leave `_recording_error()` clear; that second step could therefore enqueue a new driver free. Later steps could change a previously published Unknown into Returned while retaining the original nonzero error. Both release functions accepted some inconsistent Returned-shaped reports. Current inspected ANGE consumers reject such contradictory reports; no actual budget refund is asserted.

The original Registry now privately latches the first maintenance error before the second retire step. That child cannot enqueue new frees, but an already enqueued physical event remains observable while Unknown and the original error remain. Release requires the existing owner conditions and a no-error Returned state; it additionally refuses a latched maintenance error. Ordinary non-maintenance Unknown is not globally frozen. Native-unit reports at `UINT64_MAX` are frozen before new enqueue: a prior Pending snapshot remains Pending, the private latch blocks release, and no newly published Unknown is claimed. A previously published valid Returned/MAX can still be acknowledged. No ABI structure, public entry, native completion source, alternate owner, or synthetic physical return is added.

## Verification

Build command, rc 0: `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes`; output `build.log`. Editor SHA-256 `e58bfcdeb57d0d43938e1b8af2637c82160d8d65d907e2910278823f4b12ede6`. The linker warned that the installed MoltenVK object targets macOS 12 while the binary targets 11; this does not itself establish a runtime failure.

Before the source repair, saved RED controls demonstrate: storage and native report Unknown-to-Returned inconsistency; incorrect ack of reports with an error; storage enqueue after error; original service/maintenance fake-driver native poll failure; the post-poll shader enqueue window; first-error replacement; and unversioned change/enqueue at unit revision exhaustion. The fake poll returns an actual test error through the original service path, while real device failure was not injected. The stored red logs show individual failures and are not green runs. A separate unit reducer uses a synthetic event; the storage test additionally uses the original fake-driver dispose queue.

After repair, all below returned rc 0 with `bin/godot.macos.editor.dev.arm64 --headless --test --test-case=<pattern>`:

| Pattern | Cases | Assertions |
| --- | ---: | ---: |
| `[Qualification]*` | 42 | 648 |
| `[QualificationRt]*` | 4 | 378 |
| `[RenderingDevice][Qualification]*` | 7 | 46 |
| `[K03][P2]*` | 22 | 350 |
| `*device_exit*` | 133 | 2619 |
| `*FrameWait*` | 17 | 301 |

Controls include service poll failure before a shader becomes eligible, an already queued buffer disposal, retry after first error, synthetic native event, non-maintenance Unknown resolving from an original event, revision MAX freeze, synthetic Returned/MAX ack, each invalid status/error/latch field, and close before service with a preexisting queued/latch state. The latter is a controlled precondition, not an observed simultaneous real-service race. Report/event checks in unit-reducer/ack fixtures are not evidence of native hardware disposal.

`python3 work/qualification-retire-maintenance-unknown-20261029/run_metal_controls.py` returned 0. Existing positive and cancellation-prefix substrate controls each returned 0 with 17 checks; input hashes before/after match in `metal-results.json`. Metal 4.0 / Forward Mobile on Apple M3 Pro provided exact 104-byte positive readback, actual unit/buffer return events, bridge close and DSO finalizer/manager close. This run used previously generated source/containers (generation editor SHA `720900340909ef176290cb20da1f28b903bd1918e1e92ef5e5e54bbb0e0ba197`) with this new editor, not a newly regenerated matched compiler tuple. The run does not inject a hardware error or prove ANGE product integration.

## Independent review and remaining work

Read-only `/root/t1_independent_review` traced the original call chain, confirmed the ordering flaw and recommended latching before the second step, preserving first error, keeping non-error Unknown distinct and freezing revision exhaustion. After reviewing the implementation it found no explicit new P1/P2 in this bounded change. Its final test review confirms the controlled nature of close/MAX controls; it did not run builds/tests or edit files.

Separate existing work remains open: native-unit release currently relies on the original report masks rather than independently cross-checking every cell/frame/event against the report; a non-maintenance Unknown may expose an invalid intermediate state/reason combination before later evidence. Also, this change does not attach the native poll raw `-21` to a retirement report: preserving raw provenance requires a same-call source association for every maintenance failure path. These are not claimed fixed. The original four-method T1, B/C05 integration, U08/U02 and full acceptance remain open; the long goal remains active.
