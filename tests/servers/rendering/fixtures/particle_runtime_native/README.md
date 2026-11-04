# Existing native particle runtime fixture

This checkout entry maintains the existing `ParticlesRuntimeTestFixture.begin/advance/result` interface in the default-off `particles_runtime_tests` module. It does not introduce another C++ fixture or modify production/module registration. A native build must explicitly enable that module with `tests=yes` and 3D. Host execution requires scheduling authorization from the current coordinator; preparing or importing this runner does not launch it.

```sh
python3 tests/servers/rendering/fixtures/particle_runtime_native/run_native.py \
  --godot /absolute/immutable/editor.arm64 --sha256 <actual-host-sha256> \
  --controlled-metadata /absolute/controlled_build.json \
  --metadata-sha256 <actual-metadata-sha256> \
  --out /absolute/new/evidence-directory
```

There is no default host. The host and metadata paths must be absolute; both hashes are required. Controlled metadata schema1 must be available and match actual host size/hash and the macOS arm64 executable selection. Metadata selection and source snapshots are recorded observations; neither the checkout's source hashes nor this runner invent build provenance. The build owner supplies the immutable host and accepted controlled metadata. Missing/mismatched files fail before host launch. The project and script resolve relative to this tracked entry, with no runtime dependency on SharedWork or historical work directories.

The positive entry always uses `--render-thread safe`, Metal and mobile, a64x64 window, fixed64fps and dummy audio. Three ordinary startup frames are warmup only. The180-frame polling budget can only fail; it never proves callback completion or retirement. The outer90-second process timeout also only fails. The script retains the original fixture through all six asynchronous readbacks, original-owner release and physical native-resource acknowledgments. Native failure/error, missing module/device, timeout, leaks or any false check cannot pass. No RD sync, fabricated completion, new authority, holder or native handle interface is added.

Success requires the six queued and six completed readbacks, all292 native checks true with the exact label/multiplicity Counter, failures empty,26 original allocation identities and22 native resource observations. The original native checks distinguish logical RID removal from physical return and require each original native resource acknowledgment. Metal/mobile startup and actual device metadata must agree. The exact292 Counter was verified against two historical raw Metal runs (`20261101-122958` and `20261102-193655`) and the current C++ fixture's tokens match the reviewed historical implementation, preserving strings. The initial historical candidate differed; it was not used as the oracle. A future intentional fixture change must review these expectations explicitly.

Exactly eleven deliberately injected `ERROR:` messages are permitted: reset sub-emitter buffer2, update emission buffer2, update frame parameters buffer6 and create fallback trail buffer1. Both message text and multiplicity must match; an additional or missing diagnostic fails. Script/crash/leak diagnostics always fail. These expected negatives are retained in full raw stdout/stderr, not discarded or globally ignored.

`--entry-negative` instead uses headless mode and requires exit41 with the exact missing-module result and no engine errors. It never reports GPU success. Use only an independently pinned module-off/older host for this control. The negative mode is not a way to accept an unavailable native device.

Each output directory must be new. The runner preserves controlled metadata, commands, actual return code, separate raw logs, expected/observed diagnostics, native result, source hashes and a file manifest. The runner itself performs no build. Passing evidence is limited to this fixed native retry/trail/readback/physical-return experiment; it does not qualify an ANGE producer/product, K03 holder, external output release, P3/P4 or an arbitrary renderer workload. No native execution is implied by static/parser controls alone.
