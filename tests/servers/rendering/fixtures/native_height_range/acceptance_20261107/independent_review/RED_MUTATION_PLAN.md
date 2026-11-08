# Native height range meaningful RED mutations

Read-only recipe, no production/test source mutation or build/run by reviewer. Apply each mutation independently to root's fixed source, run only the named focused case(s), restore before the next mutation. Assert exact replacement count before patching. Default Sol review; no Astra used.

## 1. Exact slot and buffer enrollment

In `RenderingDevice::_native_height_issue_slot`, replace exactly once:

```cpp
member.issue_attempted || member.slot != p_slot || member.command_buffer != p_command) {
```

with:

```cpp
member.issue_attempted) {
```

Run `[RenderingDevice][NativeHeightRange] exact slot buffer and empty BUSY`.

Expected precise failures: after `issue(f, 1, 45, OK, report())`, original `range->source_count == 0`, `CHECK_FALSE(range->graphs[0].issue_attempted)` and later `f.driver.retains == 0` fail, because wrong-slot enrollment attached serial 71. For separately proving buffer matching, use the narrower independent mutation removing only ` || member.command_buffer != p_command` from the same unique predicate; the second call `(2,46)` then causes the same assertions to fail. Removing only ` || member.slot != p_slot` tests the first call independently. Combined deletion is one RED build; separate deletions give stronger discrimination when build cost permits.

No arbitrary driver serial or real RID is introduced; the synthetic fact still uses serial 71/901, allocated source capacity is 1, member becomes attempted once, and fixture destructor explicitly clears synthetic RD/Frame refs then detaches driver. Failures use CHECK and continue safely.

## 2A. Ordinary actual issue-hook enrollment

Replace exactly once:

```cpp
const Error height_issue = _native_height_issue_slot(frame, frames[frame].qualification_epoch, i, command_buffer, queue_error, report);
```

with:

```cpp
const Error height_issue = OK; // RED: omit ordinary actual enrollment.
```

Run `[RenderingDevice][NativeHeightRange] ordinary and bounded actual issue hooks retain before recycle`.

Expected failures in the ordinary (`bounded=false`) iteration: source_count==1, issue_attempted, eventual sources[0].retained, observe==OK, native_complete and give_back==OK. The driver issue count remains 1, isolating missing enrollment from missing actual submission. The bounded iteration remains the positive counterpart. The unpopulated source array still has its prepare-time allocation, so sources[0] access is safe even though source_count is zero. Original frame poll returns BUSY on the default incomplete callback, preventing deeper recycle/fence code. No GPU or destructive finalizer is involved.

## 2B. Bounded actual issue-hook enrollment

Replace exactly once:

```cpp
const Error height_issue = _native_height_issue_slot(frame, f.qualification_epoch, slot, command, error, report);
```

with:

```cpp
const Error height_issue = OK; // RED: omit bounded actual enrollment.
```

Same focused case, expected analogous failures only in `bounded=true`; ordinary stays positive. Cursor still records actual attempted slot 1/phase 3. Default incomplete original source returns BUSY before deeper cleanup. Retain hook alone cannot fabricate a missing source.

## 2C/2D. Retain barriers: current test has a false-negative gap

Current registered test alone does NOT precisely detect deleting either retain barrier. The original source callback starts incomplete. Without the barrier, original poll independently returns ERR_BUSY and recycles stays zero, which matches existing assertions. Do not report a RED PASS/fail claim from the current test as proof of barrier coverage.

Before mutation, add the following single assertion to `actual_issue_hooks_and_retain_barriers`, immediately after its existing `CHECK(f.driver.recycles == 0);` (this is within the ordinary/bounded loop and therefore tests both paths):

```cpp
CHECK(f.driver.polls == 0);
```

This is the minimal missing contract: retain BUSY must stop before *any* original source polling/recycle. Do not change default incomplete callback; it provides a safe return stop even in the mutated code. Run baseline first.

Then replace the entire following block with an empty block, in exactly one named function at a time (function-bound replacement, not an unconstrained global replace):

```cpp
const Error height_retain = _native_height_retain_frame_sources(p_frame);
if (height_retain != OK) {
    return height_retain;
}
```

- Mutation 2C: remove only from `_stall_for_frame`. Ordinary iteration polls serial 71 once, returns ERR_BUSY as before, and the new polls==0 assertion fails. Bounded iteration passes unchanged.
- Mutation 2D: remove only from `_native_disposal_stall_step`. Bounded continuation phase 3 polls during the first maintenance call and again on explicit stall, so polls==0 fails. Ordinary iteration passes unchanged.

Both preserve driver issue count 1, no recycle, zero fence-path advancement and the original incomplete callback's BUSY, so there is no new crash/teardown dependency. After retain_busy=false, test explicitly retains and changes original callback to complete before observe/return exactly as the baseline does. Fixture cleanup stays synthetic and detaches driver, avoiding LAST_REF destructive finalization.

## Required evidence

Root should keep fixed-source before/after hashes, one exact mutation patch per build, baseline focused output and each RED focused output. Accept RED only for named assertion failure(s), not compile failure, signal/crash, timeout, unrelated engine error or teardown failure. Restore and run focused baseline after the final mutation. GPU must run only on restored fixed source; its clean ordinary/bounded evidence remains separate from synthetic RED controls.
