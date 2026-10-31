# SortEffects failure propagation harness

`run.py` extracts the current `SortEffects::sort_buffer` method and compiles it against a small fake RenderingDevice. The fake device records compute-list lifecycle and dispatches, and can mark the uniform set or each sort pipeline invalid.

This is a **CPU scheduling harness**. It does not compile or execute the sort shader and cannot establish GPU ordering or rendered output. Its purpose is to pin the fail-closed contract:

- invalid uniform set, block pipeline, step pipeline, or inner pipeline must return failure and perform no compute-list begin or dispatch;
- zero/negative sizes and expired nonzero uniform RIDs must reject before beginning a list;
- rejected compute-list begin must return failure without dispatch or end;
- sizes up to 512 must work when unused merge pipelines are invalid;
- valid sizes 1, 512, 513, and 1025 must preserve the existing dispatch schedule;

This harness covers the sort method. It does not execute the particle caller, prove that a failed sort suppresses later instance copies, or prove that previously cached instances cannot draw.

Run the expected RED against a revision that still has `void sort_buffer` and no validity guards. The checked-in production revision is passed explicitly so a later worktree cannot accidentally turn RED into GREEN:

```sh
python3 tests/servers/rendering/fixtures/sort_failure/run.py --source-ref cff520c3c8 --expect-red
```

The cff520c3c8 RED run reports eight rejected-input/lifecycle cases that still dispatch or return success. This is expected evidence for the pre-fix behavior, not a failing test of the current implementation.

The command exits zero only when the harness fails as expected. After changing the method to return `bool`, validating all required RIDs/pipelines before `compute_list_begin()`, and returning false on rejection, run the GREEN check:

```sh
python3 tests/servers/rendering/fixtures/sort_failure/run.py
```

The harness intentionally treats the production signature as an implementation detail and appends a successful return only for the current `void` version. This lets RED be captured before the production edit while retaining the same behavioral assertions for the `bool` contract.
