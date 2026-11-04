# Independent guard numeric GPU probe

Finite9-case probe passes on canonical source94b06dc4876d153492aa0c0e2f3457d2f5afc92e and fixed attempt-03 host SHA256965cb0ef6e137dcf17298d504f404a6f53aaa0421d03d080ef36e9b5b9b24eb6. This probe started only after all17 engine tests and the basic array-length/guard native Metal runs passed cleanly. No production source or old12-case guard evidence was modified.

`FROZEN_ORACLES.json` fixes all literal status/commands **before GPU execution**. A separate scalar translation of the production guard first passes the same9 CPU examples (`cpu/reference.log`). The GPU run is `runs/20261104T051308Z-8c0941a6-metal`, using real Metal4.0/Forward Mobile/Apple M3 Pro(Apple9) main RD. Matching BUILD_IDENTITY is copied in that run; shader/SPIR-V/input/script/host hashes, command, exit and raw data are retained.

| Case | Literal expected/observed fault | Literal expected/observed command |
| --- | --- | --- |
| Identity healthy | 0 | `[6,1,0,0,0]` |
| Largest finite custom F32 bits7f7fffff | 0 | `[6,1,0,0,0]` |
| Quiet NaN custom7fc00001 | 16 | all five words0 |
| Positive infinity custom7f800000 | 16 | all five words0 |
| Negative infinity customff800000 | 16 | all five words0 |
| Positive subnormal custom00000001 | 16 | all five words0 |
| Negative subnormal custom80000001 | 16 | all five words0 |
| Finite multiplication overflow | 16 | all five words0 |
| Finite addition overflow | 16 | all five words0 |

Every healthy status is exactly `[0,1,0,0,55,0,1,0]`; every numeric rejection is exactly `[16,0,0,0,55,0,1,0]`. There are zero deviations, exit0 and no unexpected ERROR/fatal diagnostics. Each row completes two actual asynchronous buffer readback callbacks (18 total). The finite-multiply case uses a largest-finite affine coefficient with mesh x in[2,3]; the finite-add case uses the largest-finite coefficient and translation with mesh x in[0.5,0.75]. All scalar inputs in those two cases are finite; intermediate arithmetic overflows. The literal numeric-fault oracle remains16, not an observed-value-derived bounds-fault allowance.

The fixture calls default shader_create_from_spirv only and changes no compile option. Actual shader_get_compile_flags reports requested0 for all five pipelines; the log preserves each value. This observes the requested default policy, not a general proof of effective MSL math behavior. No force-safe option or algorithm fix was applied to obtain these results.

This is finite shader-guard mechanism evidence on one device/driver. It does not certify all numerical inputs, outward-rounding cases, material transforms, original producer algorithms, E09 source authority, full numeric profiles, native wrapper dispatch, renderer adoption or production qualification. CPU translation success is listed separately from GPU execution. Async readback introduces observation/synchronization; it is not no-stall scheduling evidence. Public frees are cleanup requests only; no native physical-disposal observation is claimed.
