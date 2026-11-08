# Main-RD guard pipeline preparation fixture

Build the explicitly enabled `particles_runtime_tests` module with `tests=yes` and 3D enabled through the canonical engine build. The module remains disabled by default. This adds a separate fixed class; the existing `ParticlesRuntimeTestFixture` is unchanged.

Run a new matched host with its explicit actual hash:

```sh
python3 tests/servers/rendering/fixtures/particle_external_guard_native/run_native.py \
  --godot /absolute/path/to/editor.arm64 --sha256 ACTUAL_HOST_SHA256 \
  --driver metal --out /absolute/new/result-directory
```

`--driver vulkan` selects a separate actual-backend run. There is no default host, supplied RID, shader source, operation token, budget grant, holder or fault target. The GDScript creates `ParticlesExternalGuardTestFixture` and schedules its parameterless `run()` on `RenderingServer.call_on_render_thread`; `result()` only returns a synchronized deep snapshot. No native RID or backend handle is exported.

The fixed callback checks the original main RD, correct render thread and no active list. On that owner it prepares all five real guard shaders/pipelines, checks valid original owner/driver entries and the exact shader association/compute layout, checks all 12 reflected descriptors (10 for empty variants) and readonly source roles, proves five distinct shader/pipeline RIDs, verifies repeated prepare preserves those same RIDs, releases, verifies both empty getters and original RD owner-entry removal, prepares all five again, then releases the owner again. Completion is published after the wrapper's render-thread destruction scope. The independent driver requires exactly 270 unique expected check labels, all true; missing, duplicate, extra or false entries fail.

The launcher pins the actual host SHA before and after, requires exact requested backend/device report and real backend startup, process exit0, exact PASS and report, and rejects error/assertion/crash/leak diagnostics. It retains raw stdout/stderr and JSON results. Its external process timeout is180 seconds. GDScript's120-second poll is only a diagnostic deadline: it cannot interrupt a synchronous or stuck render-thread callback, especially with a single-threaded RenderingServer. Timeout never becomes a pass or inferred resource return.

To prove missing module registration against an old/disabled host without starting GPU preparation:

```sh
python3 tests/servers/rendering/fixtures/particle_external_guard_native/run_native.py \
  --godot /absolute/path/to/old-editor.arm64 --sha256 ACTUAL_OLD_HOST_SHA256 \
  --expect-missing-fixture --out /absolute/new/registration-red
```

That separate headless control must return exactly77 with the missing-class marker and no script/parse/fatal error. It is a registration RED, never native GPU evidence.

Logical owner removal does not mean the driver deferred-free queue returned. This fixture records no dispatch, allocates no data/source buffer or uniform set and claims no physical return, SourceBridge, producer admission, renderer adoption or overall B4 acceptance. Main-RD GPU preparation must be run against the new canonical host; syntax checks and the module-off control do not replace it.

The private Count+four-gate successor also checks the independently engine-owned bundle and its survival after local wrapper release. Each engine variant produces one aggregate distinctness label, so the exact-label oracle rejects duplicates as well as omissions. These 270 labels supersede the historical 90-label wrapper-only oracle; an older host is not an acceptable matched qualification host. This change adds no dispatch or physical completion claim.

The material observation cases exercise the pointer-free original material/generation/shader-version/inverse projection, unchanged output on unissued/returned/invalid selection, retained-source replacement isolation, independent return of two selections on the same material, and real material/shader mutation after return with stale-generation and dirty-selection refusal. Replacement isolation does not establish Host resource copy-on-write or product material/profile qualification. Both Mobile and Forward+ use this fixture (`--method mobile` or `--method forward_plus`).
