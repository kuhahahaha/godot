# Pipeline recompile drain

Forward+ and Mobile drain their accepted pipeline compilation tasks before mutating ShaderData or freeing a failed shader version. Published pipelines retain their original worker task ID until an actual wait; failed tasks are reclaimed once, and empty failure placeholders are not passed to RD free. Native compilation remains parallel. Canvas is unchanged.

Actual RED used the previous task ledger with the independent empty-placeholder guard: 3 cases, 2 failed, 7 failed assertions. Fixed runtime: 3/3 cases and 50 assertions passed, exit 0, empty stdout and stderr. Tests use the original WorkerThreadPool, semaphores, task IDs, and real cache methods. The 28-source incremental build passed with unchanged frozen inputs; two mechanical-check passes passed and were idempotent.

DELIVERY.json records the four source hashes and host composition. Raw RED is in work/b4-actual-lock-pipeline-red-20261105; fixed XML and process evidence are in work/b4-core-window-lock-pipeline-green-20261105/runtime/pipeline. Independent review found no new defect in this task lifetime change.

This is CPU task and cache lifetime evidence. Live GPU dispatch, actual shader compiler qualification, platform CI and TSAN are not established by these tests. The combined request fixture still has a separate release-sequence failure under investigation; it is not reported as passing.
