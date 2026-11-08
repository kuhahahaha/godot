# Independent review: aggregate hold increments

Reviewer: native_facts_transport_sol, independent author context, 2026-11-08. Exact incremental patch SHA256 `f63bd2cb93dff46f5500387b1d92e4f181b3c26e7af4389a176fe7ee38c8e9fc`.

Verdict: no source-correctness blocker found within the private aggregate count/eligibility prerequisite and its stated exact-owner-input preconditions. This does not establish full G or preparation admission. The helper has no backend production caller. Scope/authentication, complete retained geometry/material roots, material raw-root cap, symbol identity coalescing, per-group adoption/origins, capacity reservation and execution revalidation remain separate requirements.

## Source checks

- `_native_immutable_hold_capacity_locked(n)` uses subtraction before comparison: original uint32 device counter must be at most MAX−n. Existing one-hold acquisition now calls this same predicate with1, preserving its old counter==MAX boundary. Zero planned holds is an allowed no-op here, and the new private helper caps plans at3 before converting/counting.
- `_native_immutable_resource_eligibility_locked(rid,n)` explicitly rejects0; MAX−n is safe for every uint32 n. All old escaped/retiring/native-driver/writability/dynamic/return-closed checks remain after the common counter lookup. Existing calls retain the default1 behavior.
- Each intended hold obtains its own deduplicated original closure using `bidirectional=false`, matching actual `native_immutable_hold_acquire` traversal. It validates each leaf limit1..1024. The count loop counts membership once per distinct hold, regardless of duplicate roots or multiple paths within that hold. Original key validation guarantees an actual RID has future=MAX, so equality does not split one actual counter across arbitrary future IDs.
- Every actual RID from every planned closure receives its complete summed increment, including actual dependencies reached through a future VA/pipeline. Rechecking the same actual counter from another closure is redundant but produces the same decision and no effects. Future symbols have no existing owner counter; at most3 newly planned holds cannot overflow a new uint32 counter. This depends on the documented coalesced factory identity and exact root/dependency input, not caller descriptor equality.
- Only local closure Vectors change. No driver factory, RID owner, hold counter, staging cell, reservation or origin is written by the aggregate helper. Scratch allocation failure returns through the existing checked traversal. A successful return reserves no capacity and is not a replayable permit.

## Test review

The case uses two actual vertex-buffer owner records and real typed traversal; only near-saturation owner/device metadata is injected. Literal expectations independently cover duplicate root=one increment, two planned holds at MAX−1 rejecting, MAX−2 accepting two but rejecting three via a future VA backing edge, MAX−3 accepting three, and device MAX−1 accepting one/refusing two. The symbolic VA plus a/b leaf bound2 versus3 is independent of the aggregate arithmetic oracle.

The fixture asserts injected counters before resets, checks zero state before the first injection, and restores injected state before normal free/dispose. It checks no driver allocation/free, VA/format, or dependency-map effects during the helper. Assertions use CHECK and guarded nullable owner access, with no exception-dependent REQUIRE/throw/try.

Both RED controls attack real shared predicates. Removing aggregate resource arithmetic should fail literal two/three-plan refusals; removing aggregate device arithmetic should fail the two-plan device refusal. These are source sensitivity predictions only. The test supplies root plans at a private friend boundary and does not authenticate a real preparation row, original material root snapshot, scope, or factory execution.

Nonblocking coverage follow-ups: empty plan list/no-op semantics, four-plan rejection, zero leaf limit, and eligibility failures such as escaped/retiring actual resources are not separately asserted by this new case. Existing shared-predicate tests may cover individual eligibility behavior; future orchestrator acceptance must independently cover original material raw-root caps and per-group origin failures.

## Independently performed verification

All three before/after SHA256 pins matched. `git apply --check` against current live f6ca49271b worktree exited0. Temporary replay against frozen before copies exited0 and byte-matched all three after copies. Both RED patches apply-checked against that temporary green after-state with exit0. No live source was changed. No compile, syntax check, build, doctest, mutation test or GPU operation was run by this reviewer. Exact patch SHA was confirmed during these checks.
