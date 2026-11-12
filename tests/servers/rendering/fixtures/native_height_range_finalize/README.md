# Original RD finalize / LAST_REF subprocess controls

This isolated CPU fixture tests the actual `RenderingDevice::finalize()` entry twice and the actual destructor's LAST_REF route. It reuses the original RD range reducer and a finite fake-driver native call; it is lifecycle evidence, not GPU evidence. The separate Metal range fixture supplies real graph/native membership acceptance.

The heap RD has one Frame, one retained exact synthetic source, no open command buffer, no fence and no pending resource allocations. `finalize()` therefore runs the real frame/worker drain and reaches observer return. A test driver returns `ERR_BUSY` from observer release. The test never writes `native_height_finalization_drained` or `native_complete`. Both actual finalize refusals must preserve the same driver/range, leave native_finalization_returned false, hold the return cursor, and make two release attempts with no extra issue/end/poll/recycle.

The test driver puts a sentinel in `command_pool_free`, the first dependent teardown call for this empty Frame. Reaching it exits 87 before any fake/null context could be dereferenced. The accepted blocked path exits 0 without destroying unresolved owners; process isolation is intentional. `last_ref` instead calls actual `memdelete(rd)`: the original destructor retries finalize and then `require_close_success` exits 78, printing the source-verified `GDEXTENSION_CLOSE_FAILED error=044 route=3` (ERR_BUSY=44, LAST_REF=3). A returning destructor exits 88; any fixture assertion exits 89. The default full-suite case has no environment mode and does no destructive work. It does not replace or clear an existing RD singleton.

Run after root builds the test host:

```sh
python3 tests/servers/rendering/fixtures/native_height_range_finalize/run_finalize_entry.py --host /absolute/path/to/godot-test-host --out /absolute/path/to/evidence/finalize-entry
```

For a controlled RED, root removes only the finalizer's `height_return != OK` guard after the real helper returns BUSY, builds a separate host, and runs:

```sh
python3 tests/servers/rendering/fixtures/native_height_range_finalize/run_finalize_entry.py --host /absolute/path/to/red-host --out /absolute/path/to/evidence/finalize-entry-red --expect-guard-removed
```

The RED must exit 87 at NHR_DEPENDENT_FREE and never reach the blocked-retry marker. Restore original production sources before final pins. The author does not perform the mutation/build here.

Limit: because this Frame is idle after original source recycle, stable driver counters cannot independently distinguish a second empty flush from the correct post-drain phase skip. The test checks the real phase flag and safe repeated refusal. No production counter/setter was added to manufacture stronger coverage. A future actual-Metal teardown child could strengthen that part without changing this result's scope.

Child-mode driver retain, poll, recycle and release serial invariants use a shared fatal `child_require` (exit89), rather than nonfatal doctest checks. The runner rejects this marker and failed CHECK/REQUIRE diagnostics in both GREEN and controlled RED modes; expected original finalize-BUSY ERR_FAIL diagnostics remain admissible. Normal CPU cases retain their regular doctest checks.
