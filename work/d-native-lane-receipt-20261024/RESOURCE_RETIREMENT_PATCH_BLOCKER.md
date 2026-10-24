# Resource retirement error propagation blocker — 2026-10-24

The saved four-hunk patch `resource-retirement-error-propagation.patch` cleanly applies in a worktree check, but cannot be staged safely against the current mixed index: `git apply --cached` rejects `servers/rendering/rendering_device.cpp` because the index base differs from the dirty migration already present in the worktree. The file also contains unrelated uncommitted renderer/device changes.

The patch only changes deferred compute-pipeline, uniform-set, shader, and texture `observe_free` calls to set `capture_test_coverage_failed` when duplicate/stale free evidence is observed. No hunk was committed, so no unrelated dirty code was captured.

No full build was retried. Runtime/native/GPU/PCK/production qualification remains false. Safe next step is to integrate the existing registry migration or stage an isolated clean worktree before applying this patch.
