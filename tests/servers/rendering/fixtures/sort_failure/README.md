# SortEffects failure propagation and dispatch-capacity harness

`run.py` extracts `SortEffects::sort_buffer` and compiles it against a small fake RenderingDevice. The device records compute-list lifecycle, pipeline bindings, push constants, dispatches, and barrier epochs. Uniform/pipeline validity and the runtime X-group limit are controllable.

This is a **CPU scheduling harness**. It does not compile or execute the sort shader, allocate particle data, or establish GPU ordering or rendered output. It checks:

- invalid uniform set, block pipeline, step pipeline, or inner pipeline returns failure without beginning a compute list or dispatching;
- zero/negative sizes and expired nonzero uniform RIDs reject before beginning a list;
- rejected compute-list begin returns failure without dispatch or end;
- sizes up to 512 work when unused merge pipelines are invalid;
- valid sizes 1, 512, 513, and 1025 preserve the original dispatch schedule;
- capacity 0 rejects before beginning a list;
- capacities 1 and 2 correctly chunk sizes 1025 and 2049;
- capacity 65535 correctly chunks size 16777217, whose padded merge stages require 65536 logical groups, using dispatch metadata only.

The original 13 rows remain, with six capacity rows added. An independent merge-width oracle checks every complete logical stage: pipeline and merge parameters, total element count, contiguous group offsets beginning at zero, exact X chunk sizes bounded by the device limit, Y/Z group counts equal to one, and a fresh push constant for every dispatch. Barrier epochs require exactly one barrier between complete stages and none between chunks within a stage. The large boundary case covers 153 stages in 305 dispatches with 152 barriers.

This harness covers the sort method. It does not execute the particle caller, prove that a failed sort suppresses later instance copies, or prove that previously cached instances cannot draw. Its barrier checks establish recorded CPU command placement, not actual GPU synchronization.

Run the current worktree GREEN check from the repository root:

```sh
python3 tests/servers/rendering/fixtures/sort_failure/run.py
```

For historical RED evidence, select the pre-chunking source revision explicitly. This command compiles that historical method and exits zero because the six capacity rows fail as expected:

```sh
python3 tests/servers/rendering/fixtures/sort_failure/run.py --source-ref eb1644f99f8e6f11acddb2d323170e3fda38f443 --expect-red
```

`--source-ref` selects production source only; the harness assertions come from the current `run.py`.

The older pre-validity-guard revision can also be checked explicitly:

```sh
python3 tests/servers/rendering/fixtures/sort_failure/run.py --source-ref cff520c3c8 --expect-red
```

That historical revision additionally fails rejected-input/lifecycle checks. The harness appends a successful return when extracting a `void sort_buffer` method so historical source can compile against the same behavioral assertions. `--expect-red` inverts the executed harness result: it exits zero only for a behavioral failure; compilation errors remain errors.
