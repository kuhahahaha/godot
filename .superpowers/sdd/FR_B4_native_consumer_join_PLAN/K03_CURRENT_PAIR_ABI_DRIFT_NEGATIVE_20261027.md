# K03 current-pair ABI drift negative receipt — 2026-10-27

## Pair and command

- Godot editor SHA-256: `b64fd2c4a2697ffacdb04ba848d4aad19fa52097322957073551ea90a75a395e`
- ANGE DSO SHA-256: `c82f09f62b84af0ac14965a7bab29ab62e408aa5106d3264a2be7eb91777cff0`
- Renderer: non-headless Metal/mobile, Apple M3 Pro, render-thread safe.
- Fixture: the current minimal VFX and no-VFX U01 fixtures under `/tmp`.

## First causal error

Both probes terminate during GDExtension initialization before world entry or
VFX loading:

```text
ERROR: Attempt to get non-existent interface function: 'device_exit_register_scene_ready_v2'.
ERROR: GDExtension initialization function 'ange_init' returned an error.
U01_EXTENSION_STATUS=1
U01_EXTENSION_OK=false
```

- VFX probe: rc=2; log SHA-256 `4f80e1d04d2ab511ba7819c7baafd83b1c98d4e73d8a51430794a389c813ad7b`.
- no-VFX probe: rc=2; log SHA-256 `30c430ec733644c8883e5d632a095db5ed65f3f3205d7650e5d66f0d4d08c1c4`.

The logs also contain an environment-only shader-cache-folder warning before
this line; the first causal ABI error is the missing interface below. This is
an ABI pair mismatch at extension initialization. It is not evidence
of VFX source failure, U01 world failure, close failure, completion, readback,
Ready, or GPU behavior. No fallback or interface weakening was attempted.

## Next owner/tool boundary

The editor must be rebuilt or replaced with a host exposing the DSO's exact
`device_exit_register_scene_ready_v2` interface before a fresh non-headless
VFX/U01/close probe can proceed. The DSO is not changed to target an older
interface because that would weaken the typed-close contract.
