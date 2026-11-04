# Independent System scalar native driver — 2026-11-04

The new standalone driver executes the original System scalar native API on a real render thread, then observes native completion, copies the exact four GPU-produced bytes, releases the ticket, retires its original unit/storage and closes the original session/bridge. Both Metal and Vulkan completed this sequence on the retained no-trace Godot baseline. No Godot production changes, ANGE DSO, or engine build were required.

## Reproduce

```sh
python3 work/native-system-scalar-driver-20261104/run.py --driver metal
python3 work/native-system-scalar-driver-20261104/run.py --driver vulkan
```

The runner compiles only the small native controls. It pins the executable and controlled-build metadata, checks actual monolithic linkage, removes inherited dynamic-library overrides, compiles shader containers on the selected real backend, and retains each run's input snapshot and DSOs in its own result directory. Later runs do not overwrite earlier DSOs, containers or snapshots.

Host executable SHA-256 is `887bff69ab84ab6f35a88f2f2bb9cd46d5970bb7e4ca7a230ea5c94761130392`; controlled metadata SHA-256 is `c7355cb7415dcc2584522d9a2f2bbbf663fe591aec46c24a3fd655eea297293a`. Its selected core stamp is `1da8f74011c7992723d1cd5c47aaacc35f480898ad8a2a3bb1cdd97e1ea9548b`. This is the previously built no-trace executable, not a new build of all current dirty sources.

## Verified runs

| Backend | Results directory | Container / wrong-expectation control / positive exits | Runner checks |
| --- | --- | --- | --- |
| Metal 4.0, Forward Mobile, Apple M3 Pro | `results/20261104-082753-metal` | 0 / 180 / 0 | 10/10 |
| Vulkan 1.2.357, Forward Mobile, Apple M3 Pro | `results/20261104-082803-vulkan` | 0 / 180 / 0 | 10/10 |

Each wrong-expectation control fails exactly at `scalar_exact_output`, rather than at an unrelated entry or runtime failure. It expects `0x40400001` from a shader that writes `0x40400000`; no expected output is uploaded by the CPU. Positive readback is exactly four bytes containing binary32 3.0 (`0x40400000`). The retained output allocation has original backing serial 13; Body and readback facts have successful original native issue/completion serials (Metal 12/12, Vulkan 7/7), nonzero queue and no error. Metal additionally reports its original copy-queue source 11/11. Different backend fact counts are preserved.

The native C++ asserts source range/backing identity, current record generation/request nonce, nonzero original async request, one Body and one exact four-byte readback, all facts completed without possible/unknown native effects, exact bytes, completed-result cancellation refusal and single ticket acknowledgement. It checks original unit shader/pipeline free-event fields and storage physical report before their acknowledgements. Guard user/binding cleanup completes before the DSO unload marker. All final input/snapshot/DSO/log hashes were independently rechecked; host and source bytes remained unchanged.

## Real thread and lifetime integration

SceneReady binds the original main device/session and opens its bridge with the retained CodeHolder. Original E05 storage and native unit creation use the existing QRT. The driver creates a custom Callable and sends it through public `RenderingServer.call_on_render_thread`; the callback verifies `is_on_render_thread()` before invoking `device_session_system_scalar_record_v1`. No synthetic QRT kind is introduced. The owner waits for the callback's atomic return, then calls the original System observe/facts/copy/release interfaces.

The guard declares both SceneReady and its custom-Callable entry class. The first development run omitted the Callable bit and was correctly refused with `UNSUPPORTED_ENTRY_CLASS`, before recording. Its failed input snapshot and raw logs remain under `results/20261104-082651-metal`. Adding the same bit used by the existing `qrt_owned_cleanup_probe.cpp` fixed the test registration. Neither the original guard policy nor the engine was changed. Final runs rebuilt both controls from the corrected source and used fresh result directories.

System ticket release succeeds only after the existing `qualification_release(..., true)` internal-set `free_event` gate. The driver does not directly expose that internal event or provide a mutation that independently removes this gate. Successful release plus the current production gate supports the internal-set return conclusion; the driver directly asserts physical reports only for its native unit and storage.

## Independent review and remaining scope

`/root/system_driver_review` checked the final C++/runner, both actual runs, five input hashes and their snapshots, two DSO hashes per run, all logs and the deliberate failures. Review accepts this as independent System scalar native boundary evidence and explicitly retains the internal-set-event limit above.

The driver uses local control identities and a minimal constant-writing shader. It does not certify an authentic ANGE request/owner association, ordinary World caller wiring, Method semantics, System active cancellation, Host-before-World closure, live particle pixels, platform release builds or arbitrary scalar shaders. Those remain separate product obligations. Vulkan's full V2 qualification capacity remains unavailable: System scalar follows its own original native path, so this success does not change or bypass that separate gate.
