# Fixed native owned-prefix disposal fixture

This default-off `particles_runtime_tests` module requires `tests=yes` and 3D. The script invokes only parameterless `run()` and synchronized `result()`. It accepts no RID, source, producer, budget, authority, holder or fault target. No native identity leaves the fixture.

Use an explicitly approved, immutable host and its SHA-256:

```sh
python3 tests/servers/rendering/fixtures/particle_external_output_native/run_native.py \
  --godot /absolute/path/to/approved/godot --sha256 <host-sha256> \
  --out /absolute/path/to/new/evidence-directory --driver metal
```

`--driver vulkan` selects an independent actual-backend run. `--expect-missing-fixture` instead runs a headless registration RED on a known module-off or older host; the expected exit is 77 and it never proves GPU execution. Host launch still needs the task's explicit scheduling authorization.

The fixed allocation is capacity 1 / surface count 1: seven buffers of 192, 192, 32, 64, 32, 20 and 384 bytes, totaling 916 bytes. A single original main-RD render-thread callback observes LIVE, rejects premature destruction, retires through the original owner, requires ENQUEUED with no RETURNED before maintenance, checks idempotent retirement, then calls the existing nonblocking maintenance until all seven original witness identities are RETURNED. Only the original driver's free-return path writes those witnesses. Controlled destruction must then clear the pointer.

Disabling the render loop alone does not stop Main from calling `draw(false)` for pending frees. The entire interval stays in one callback, with no command-queue pumping, manual draw or RD sync. Engine draw count must stay unchanged; internal RD frame advances are expected. The callback has a 20-second failure deadline and the outer process timeout is 180 seconds. Neither a deadline, iteration count nor frame count can infer completion. The 27 exact labels in `run.gd` and `run_native.py` must each appear once and pass.

UNKNOWN, a non-BUSY maintenance error, timeout or assertion failure stops acceptance and retains the raw prefix pointer. The fixture never resets UNKNOWN, manually drains pending queues, fabricates completion or frees from a destructor. The failure report is final even if later engine teardown disposes resources. Process exit is not evidence of successful retention across device teardown. This fixture does not acquire or validate a K03 holder; the successful experiment ends before its single callback returns. No production scheduling, admission or budget path is added.

A successful result claims only `physical_prefix_returned`: original driver-free calls returned for these unused, Core-created buffers. It does not prove producer-use retirement, dispatch completion, OS memory reclamation, full output ownership release, budget discharge, K03 device-holder behavior or whole P4/K03 acceptance. `whole_output_released`, `source_authority_supplied`, `dispatch_recorded` and `device_holder_claimed` remain false.

The runner rejects missing/duplicate labels, backend mismatch, malformed reports, fatal/leak diagnostics, timeout and host/source hash changes. It records a bounded source manifest and host hash; source hashes are observations, not proof that those sources were compiled into the supplied host. Build provenance must be supplied separately by the canonical build owner.

Template binaries may emit the single startup diagnostic `ERROR: Could not load global script cache.` before loading this script when no editor cache exists. The runner filters only that exact line; all other `ERROR`, assertion, crash, leak, and script diagnostics remain fatal.
