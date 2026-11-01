# Active Reload continuation source audit

Date: 2026-11-01

## Scope

Audited and hardened the ordinary GDExtension hot-reload instance-state capture in `core/extension/gdextension.cpp`. The production change snapshots tracked instance IDs before property callbacks and reacquires each object after callback-capable operations, skipping state capture when an object was destroyed during the callback.

## Finding

`GDExtension::prepare_reload()` previously iterated `E.value.instances` directly while calling `Object::get_property_list()` and `Object::get()` for each live extension instance. These calls dispatch extension-defined property callbacks. If a callback destroys an extension object, the normal untrack callback can erase its `ObjectID` from `E.value.instances` during the range iteration. The function now snapshots IDs before callbacks, then reacquires each object by ID after `get_property_list()` and each `get()`; destroyed instances are skipped without dereferencing stale pointers.

The nearby `GDExtension::clear_instance_bindings()` already uses a `Vector<ObjectID>` snapshot because free callbacks may untrack themselves or destroy another tracked object. This is direct local precedent for the collection-mutation concern, but it does not prove that property callbacks actually destroy objects in production.

## Executable evidence

`python3 work/active-reload-continuation-20261101/check_prepare_reload_instance_snapshot.py` extracts the current `prepare_reload()` function and checks whether it directly ranges over the callback-mutable instance set or first snapshots IDs. On the hardened source it reports:

```text
GREEN: prepare_reload snapshots instance IDs and reacquires objects after property callbacks
exit code: 0
```

This is a source-contract check, not a native runtime test. It verifies the snapshot invariant but does not claim a reproduced engine crash.

## Verification limits

- No engine build was made.
- No native hot-reload fixture exercised destruction from `get_property_list` or `get`.
- Existing typed-close, G1, DSO image lifetime, and complete Active Reload qualification are outside this constituent.

## Independent final review (reload_final_review, 2026-11-01)

Reviewed base HEAD `98d315818db624e4b81f867f718ec5fac9beb385` plus the existing dirty `prepare_reload()` patch. No additional production change was necessary for the bounded caller-side snapshot/reacquisition contract. Do not interpret this source review as complete Active Reload acceptance.

### Class map and default lookup

- `HashMap` stores separately allocated linked elements (`core/templates/hash_map.h`, `_insert`, `_resize_and_rehash`). Inserting a class or rehashing does not move the current `E.value`. `prepare_reload` marks the current class `is_reloading` before dispatch; `_unregister_extension_class` retains the corresponding extension-map entry when this flag is set. Ordinary object destruction only erases an instance ID. These paths do not justify adding another class-map snapshot.
- Arbitrary nested class teardown/re-registration, `finish_reload`, or abort that erases/replaces the current class is **not** covered by the instance-ID snapshot. This review does not establish that such arbitrary mutation is safe. The ordinary manager reload cannot successfully reenter through an active extension callback because its preflight sees an active callback and returns Busy.
- `ClassDB::class_get_default_property_value` can instantiate an uncached default object, call its list/get callbacks, and destroy it. Copying `class_name` before that call and reacquiring the original object after it are necessary. The current patch does both. Reacquisition before every subsequent getter and before state publication also covers destruction caused by local Variant cleanup between iterations.
- The outer checks do **not** make self-destruction inside `Object::get_property_list` safe: that function uses `_extension_instance`, `_get_property_listv`, and other receiver state after dispatch. Nor do they make default-instance self-destruction inside `ClassDB::class_get_default_property_value` safe: ClassDB continues to use and later delete its own `c`. Those internal functions are unchanged. The new list test destroys a sibling, and the default-value test destroys the original object while leaving the temporary default receiver alive.

### Behavioral evidence and native regression

Added `device_exit::method/reload-capture-survives-property-callback-destruction` in `tests/core/object/test_device_exit_method.cpp`, reusing the existing `TestDeviceExitMethod::Access` friend and `GDExtensionFunctionLoader` fixture. Its three branches call the production `prepare_reload` with real extension callbacks and ObjectDB identities:

1. A property-list callback destroys a later tracked sibling. Every surviving object's list callback must execute once and have saved state; the destroyed object's state must be absent. In particular, the last HashSet element is moved into the erased slot, so direct iteration can call that survivor twice.
2. A successful getter destroys its own receiver and returns true. No state for its dead ID may be published.
3. An uncached default-instance getter destroys the original object. The default instance remains alive; no state for the dead original may be published.

The engine test is **not run**. `bin/godot.macos.editor.arm64 --headless --test --list-test-cases --test-case='device_exit::method/reload*'` returned 1 and explicitly reported that the binary was compiled without unit-test support. The previous `bin/godot.macos.editor.dev.arm64` is absent. No full engine build or link was attempted. Required future command on a source-matched `tests=yes` editor: `--headless --test --test-case='device_exit::method/reload-capture-survives-property-callback-destruction'`, then run the pre-patch production variant to establish actual engine red/green. No runtime-pass claim is made for this newly authored fixture.

`hash_set_callback_probe.cpp` is an independently executable primitive witness using the repository's real `HashSet<ObjectID>`, with malloc/error stubs and a standard-vector copy. It reproduces the sibling-deletion mechanism without an engine build. `HASH_SET_PROBE_RESULTS.json` records clang build rc 0 and run rc 0:

```text
direct survivor_4_visits=2 expected=2
snapshot survivor_4_visits=1 expected=1
```

This is behavioral evidence of container mutation, **not** a production `prepare_reload`, ObjectDB, DSO, or manager test. Reproduce with `clang++ -std=c++17 -I. -Iplatform/macos -O0 work/active-reload-continuation-20261101/hash_set_callback_probe.cpp -o /tmp/active-reload-hashset-probe` and run that binary. The recorded run used and deleted a uniquely named temporary binary.

### Checks actually completed

- Both `core/extension/gdextension.cpp` and the modified `tests/core/object/test_device_exit_method.cpp` passed `clang++ -fsyntax-only` with tools/tests/debug enabled. Full commands and rc 0 results are in `SYNTAX_RESULTS.json`; logs are separate. The compilation database had a stale MacOSX27.0 SDK path; the final check used the current `xcrun --show-sdk-path`. The first failed setup commands remain in `SYNTAX_STALE_SDK_RESULTS.json` and receive no verification credit.
- The source-contract checker passed; it remains only a source check.
- `git diff --check -- core/extension/gdextension.cpp tests/core/object/test_device_exit_method.cpp` passed.

### Exact unresolved manager boundary

`GDExtensionManager::reload_extension` calls `preflight_exit(RELOAD)` **before** `prepare_reload`. Both ordinary `Registry::preflight_exit` and qualified `Registry::guarded_preflight` seal `record.quiescing`; ordinary `CallbackScope` subsequently returns Busy. `Object::get` then returns NIL, while `Object::get_property_list` skips extension list callbacks. A passing direct-prepare regression therefore does not prove manager-level state capture under G1. Changing that ordering or granting a new capture authority requires the manager/typed-close contract and is outside this bounded patch. This is the principal unresolved Active Reload boundary, together with source-matched native red/green and the internal self-destruction limitations above.

Source status: **reviewed candidate for the bounded capture fix; runtime qualification pending**. No full Active Reload/G1/DSO-lifetime completion claim.

## First source-matched native run and fixture correction candidate

The root builder subsequently produced a `tests=yes` editor with build rc 0 and reported no source drift. The first actual native run failed: 1 case, 21 assertions, 15 failures; every route had `deletions=0`, `list_calls=0`, and absent captured state. Raw log: `work/particle-runtime-smoke-final-20261101/reload-native.log` / `.rc`. This invalidates any inference that the initial test fixture reached the intended callbacks; it is not a successful regression or a production red.

Root cause traced from actual setup: `GDExtension` defaults `reloadable=false`. `GDExtensionLibraryLoader` sets it from configuration, but `GDExtensionFunctionLoader` does not. `_register_extension_class_internal` installs `track_instance`/`untrack_instance` only when reloadable. The test created associated objects but never enabled tracking, leaving `Extension::instances` empty. This failure occurs before callback admission and is separate from the documented manager preflight boundary.

Prepared `reload-fixture-tracking.patch` against the frozen test file (full candidate: `test_device_exit_method.candidate.cpp`; hashes: `CANDIDATE_TRACKING_FIX.json`). No production or frozen test file was changed by this follow-up. The patch:

- Calls the public `extension->set_reloadable(true)` before `open_library` registers classes.
- Supplies the class recreate callback; otherwise the concrete route-2 class disables reloading during registration.
- Requires the extension and registered class both report reloadable.
- Adds explicit getter/default-getter/list-trigger counters and route output. All three routes must actually call getters and delete the target, and only route 2 must execute a default getter.

`git apply --check work/active-reload-continuation-20261101/reload-fixture-tracking.patch` passed. The unique root builder must apply/rebuild/run this candidate after its current physics-fixture link. Callback-bearing runtime evidence is **pending**; no native success is claimed from the initial run or static diagnosis.


## Source-matched native execution

The initial engine test fixture did not enable FunctionLoader.reloadable, so registration installed no track/untrack callbacks and the instance set was empty. Its 21-assertion run failed 15 assertions and did not exercise the target callbacks. The corrected fixture enables reloadable and supplies the original recreate callback; it asserts original class/extension reloadability and actual callback counts.

Fresh tests=yes build and execution now pass: the direct-capture case has 33/33 assertions, with actual markers `route=0 deletions=1 getters=3`, `route=1 deletions=1 getters=1`, and `route=2 deletions=1 getters=2 default_getters=1`; trigger property lists execute exactly once. The complete `device_exit::method/*` neighbor selection passes 10 cases/515 assertions. Logs are `fixture-run.log` and `../native-multireadback-20261101/reload-method-neighbors.log`. Pinned host/source-window identity is recorded in `REVIEW_SOURCE_PINS.json` and the referenced host receipt.

This verifies the direct prepare capture routes (sibling removal, getter destroying its receiver, and default getter removing the original object). The manager preflight sealing boundary, property-list self-destruction inside Object itself, default-instance self-destruction, complete hot reload, typed-close and G1 remain separate unverified consumers. The earlier source-only limitations above describe the earlier audit stage; this section records the later actual execution.

Latest direct-capture/Method neighbor replay on the fixed editor-script host `a0cd958938bebf1ac698728c38e7dee9e5b623a4466fbea48ab9e304038d792f` again passes 10 cases / 515 assertions, rc0 (`final-method-neighbors.log`). The three initialize-library callback-scope rejections belong to existing explicit negative cases. Exact source identities and host-pin reference are appended in `REVIEW_SOURCE_PINS.json`; this does not expand the bounded capture contract or close the manager/G1 boundary.
