# Independent review: static output-alias READBACK plan

Reviewed 2026-11-07. Scope: the current three-file change in `gdextension_production_request.cpp`, `gdextension_production_action.cpp`, and `test_production_request.cpp`. Read-only source/receipt review; no build, test, Git operation, or active-source edit by this reviewer. This review does not cover Astra's isolated full runtime candidate.

## Verdict

No demonstrated blocking defect in the **static-plan-only** scope. The new freeze branch accepts a lawfully prepared unbound alias plan while preserving exact producer/role/read-span association. The mechanical recording tail explicitly refuses alias READBACK with `ERR_UNAVAILABLE` before initializing capture or entering native append effects. Ordinary imported/created storage READBACK retains its previous path.

This is not runtime READBACK readiness or qualification. A successful static freeze creates neither an output allocation/witness nor a BOUND output action, recorded readback, source-return receipt, or completion proof. Full runtime support remains separately blocked by the explicit gate and needs its own original factory/witness/source-closure integration and tests.

## Source findings

### Static identities and bounds

`core/extension/gdextension_production_request.cpp:910–946` first checks `producer < copy < action_count` and `resource_slot < resource_count` before indexing writer, copy or resource. Thus the producer bound follows from the strict ordering; it does not depend on an unchecked producer array access. The earlier alias resource loop at `:873` short-circuits an out-of-range `output_action_ordinal` before indexing it.

The alias READBACK branch checks resource parent pointer/serial, STORAGE kind, OUTPUT_ALIAS ownership, exact output action ordinal; writer parent/ordinal/NATIVE_OUTPUT/source-prepared identity; copy parent/ordinal/READBACK identity; both copy readback-slot fields; nonzero in-resource read extent; and an actual original copy READ use covering the requested span.

Its producer-role loop requires a nonempty role for the same resource, `use_index < r.use_count`, and membership in the writer's declared use slice before accessing the use cell. The subtraction test is guarded by the preceding lower-bound check and short-circuit evaluation, avoiding unsigned underflow. The selected original use must have the original parent, same resource, exact WRITE access, offset zero, and size equal to the full role; role bytes and generation must match the alias declaration. READ span coverage uses the overflow-safe `range(offset, bytes, extent)` implementation at `:81`, which subtracts only after checking `offset <= extent`.

`production_use_matches_locked` at `:842` checks the copy's full use slice against `r.use_count` before iterating. Its accepted READ/READ_WRITE access and subrange semantics are existing behavior. The new role identity checks do not accept a matching resource elsewhere in an unrelated writer use slice.

Original reserve validation at `:573–621` already bounds readback/action/resource/use declarations, checks exact copy identity, and requires original producer write coverage. The freeze checks strengthen the actual accepted/prepared-row association rather than accepting a second loose DTO plan. No new unbounded index or overflowing extent calculation was found in this branch.

### Runtime gate and ordinary neighbor

`core/extension/gdextension_production_action.cpp:414–422` branches on the prepared alias resource and returns `ERR_UNAVAILABLE`. This is after original identity/freeze/list/capacity admission checks, but before `readback = &cell.capture`, capture segment/count initialization, transfer staging, `capture_inflight`, `production_recording_request`, `production_append_effect_enter_locked`, compute-list close/bind, and `_record_source_buffer_readback`. The local upload capture descriptor and bound calculations above it do not issue native work.

The gate uses the same accepted action/readback/resource indices already validated by preparation/freeze; it does not add a new caller-provided index route. Its `output_alias_prepared` check continues to reject such aliases even if a future original acquire binds them; this is deliberately a static-only gate.

The nonalias imported/create storage freeze checks and upload/copy/dispatch producer coverage below the new branch are unchanged. The record path for nonalias READBACK falls through to the original 32-bit bounds and capture preparation. The two source diffs were independently compared against the candidate's saved base files; no ordinary-neighbor behavior change was found.

### Accepted static fixture and restoration

`tests/core/extension/test_production_request.cpp:10073–10252` constructs an accepted V2 arena via `ordinal_initialize`, obtains actual source storage through original factory ports, and calls registered prepare-output and prepare-readback ports. The `particles=1`/generation/camera tuple is explicitly schema-only data: the fixture does not claim a usable particle RID or run output Create/acquire.

The fixture checks actual source match; temporarily corrupts source backing serial, source generation and source-use access; and restores each before continuing. It confirms the unbound alias is not an actual matching witness/root. Ten rejection mutations and one valid control operate on existing accepted/prepared rows. Mutations cover role generation/bytes/use association, wrong writer kind, missing source preparation, both copy-slot fields, copy READ access, out-of-span readback and producer ordering. Each saved declaration/role/readback/use/copy-slot/source-prepared field is restored after the check. Only the final valid control sets `frozen`; preceding cases do not rely on an artificial unfreeze.

For every mutation/control, the fixture checks no action progress, graphs, native captures, issued/returned readback source, imported alias or valid disposal witness. It does not install a witness, set output BOUND, or claim a recorded action. The actual source allocation is canceled/retired/observed/released through original ports, followed by original request release. The fixture does not manually erase custody for cleanup.

Coverage limits: the mutation fixture does not individually corrupt every parent/serial/index field, exercise `UINT64_MAX` spans, test original runtime gate entry, or prove physical GPU behavior. Those are limits on test evidence, not demonstrated source failures. The existing original reserve validation and checked guards above were inspected directly. Full runtime gate/capture/settlement sensitivity belongs to the isolated runtime candidate's integration fixtures.

## Execution evidence supplied by root

The reviewer read these JSON receipts and their corresponding `.log` files; the reviewer did not run them:

| Receipt | Result observed in log | Binary SHA256 |
|---|---|---|
| `native-output-continued-alias-readback-static-red-r2` | 1 case failed; 225 assertions, 9 failed; exit 1 | `b884db0616c9bd122511459e7d2637d5ea19b0bd3b8c511d50a3d73b40d1fbe7` |
| `native-output-continued-alias-readback-static-green-r1` | 1 case passed; 225 assertions passed; exit 0 | `35dc9524bafcfc8d5bf4f7ee1da32826eceb98b729c9aee6a1863c6d630aa0ae` |
| `native-output-continued-alias-readback-static-combined-r1` | 216 cases passed; 91,988 assertions passed; exit 0 | same green binary |

The RED log shows the old blanket alias refusal (`2`, ERR_UNAVAILABLE) where exact invalid-data or valid-control success was expected, including the valid plan's missing frozen state. Some role mutations are already rejected by earlier guards; RED does not prove each new guard independently necessary. It does show that the fixture detects the absent static acceptance path without fabricated native state.

Combined diagnostics contain three pipeline driver-refusal messages, one invalid-UTF8 replacement diagnostic, and three foreign-camera-source messages. Root identifies these as existing expected negative fixtures; the inspected log contains no additional diagnostics and ends in SUCCESS. This remains CPU/dummy evidence.

The earlier `native-output-continued-alias-readback-static-red.json` has exit 0 and an older binary; it is superseded by `red-r2` and is not RED evidence.

Root subsequently corrected only the freeze comment to state that READBACK remains gated; final rebuild and final receipts are pending at this review's write. No functionality changed in that comment update. The r1 receipts above establish the functional version tested by root, not a rebuilt binary of the final comment bytes. Root should bind the final source identity to its final run before committing a passing-current-binary claim.

## Source identity at review

| File | SHA256 |
|---|---|
| `core/extension/gdextension_production_request.cpp` (corrected comment) | `972b833910d6f69397d572c818e9d689d650af8f9c04f1ffbbee34215ff80c53` |
| `core/extension/gdextension_production_action.cpp` | `a0280d0b224b7b24e49f2b621fbb1bccdf9dd1dfe0039543507a91a31c84a248` |
| `tests/core/extension/test_production_request.cpp` | `be86c4c0b419545f8a535df42d280aaff6d652496271b910e6f71f498c339f62` |

Receipt JSON hashes: RED-r2 `6d7ad1f61e16dfd97b317b01b2f2ffb196f1e44760366f4af9064489559b8bd0`; GREEN-r1 `35fe195ff40c16f609f9d5cccfb107e9d6637e4b310621ad81bf3cdaf943fb8d`; combined-r1 `a6c09b318e4435ddbf7c0d51a366225da9771ec1f22f9e8536fdb453ad5c230f`.
