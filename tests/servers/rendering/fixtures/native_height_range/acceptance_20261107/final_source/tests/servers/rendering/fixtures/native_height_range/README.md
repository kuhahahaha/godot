# Original native height clear range: real Metal fixture

This tests the first standalone D32 clear recipe through the original main RenderingDevice. It does not prove a Scene/HeightFrame producer, height content, hidden/worker dependencies, whole-output release or reservation refund.

The fixture quotes and creates the original 64×64 D32 texture and framebuffer, then performs an original clear and maintenance outside the tested range. It verifies a materialized driver framebuffer and `pending_clear=false` before preparing two distinct range Refs and recording one actual clear into each in the same original Frame. Range snapshots expose actual counts, not source identities; this fixture does not claim that the two ranges share a native source. Shared-source identity, early Ref drop, unknown/error/retry injection and other mechanical branches belong to the separate CPU tests.

Two independent engine processes exercise different paths:

- Case 0 keeps the render loop disabled and performs record, budgeted maintenance/poll, explicit observer returns and both physical retirements inside one original RT callback. Each maintenance turn keeps its finite budget; internal turns do not increment the actual callback counter. The engine draw counter must remain exactly unchanged through native completion and physical return. Returning to Main between these turns would permit its mandatory pending-resource draw despite the disabled render loop, so this case never queues another callback.
- Case 1 enables the ordinary render loop only after recording. Subsequent callbacks only observe the ranges until both complete; no bounded maintenance runs between recording and native completion. The draw counter must increase. The main thread then disables the loop. A single subsequent RT callback performs all explicit observer returns and bounded physical cleanup, without yielding an unfinished native disposal continuation to Main.

The same fixture Ref survives every callback (exactly one callback for case 0). Range state, resource identities and phase are confined to the render thread. The main thread reads only mutex-protected deep snapshots and allows at most one callback in flight.

Both ranges must have actual graph members, closed mappings and issues, retained original sources and callback-established completion. At native completion, the original depth and framebuffer witnesses must still be LIVE. Explicitly returning the first range must preserve the second range's completed observation. Returning both observers must still leave both physical resources LIVE. Only then does the fixture retire the original framebuffer, observe its actual RETURNED witness while depth remains LIVE, retire depth and observe both original witnesses RETURNED with unchanged RID/serial identities. Completed range observations must remain immutable after physical cleanup. Before publishing success, the fixture finishes the original maintenance continuation and requires `native_disposal_active=false` and `_recording_error()==OK`; physical RETURNED alone does not establish that the next frame can record safely.

`run_native.py` requires an absolute engine path and its exact SHA-256:

```sh
python3 tests/servers/rendering/fixtures/native_height_range/run_native.py \
  --godot /absolute/path/to/godot.macos.editor.arm64 \
  --sha256 EXACT_LOWERCASE_SHA256 \
  --out /absolute/new/evidence/directory
```

Build with `tests=yes module_particles_runtime_tests_enabled=yes`. The runner requests real Metal Mobile and the safe render thread, validates the exact 34-check set and every stage, and preserves both process logs, their hashes, host identity and before/after source pins. Runtime source identity is not build provenance. Unsupported capabilities, unknown states, malformed reports, warnings, errors, deadline expiry or nonzero exits cannot pass. Failures retain unresolved original prefixes; there is no fallback that manufactures completion or physical return.

The fixture has been implemented for parent-controlled build and real GPU validation. Static checks alone are not acceptance evidence.
