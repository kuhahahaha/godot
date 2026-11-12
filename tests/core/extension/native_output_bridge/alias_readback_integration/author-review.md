# Author source audit: internal alias acquire/readback fixture

This is an author audit, not independent review. No build or execution was performed by this agent. Active source was read only; root owns application and verification.

Reviewed test file SHA256: `abed00e018184cb3008682ad1329c4f5ae7d9ccf4a53fe20a7c4994d76f13d79` (line references below refer to this snapshot).

## Findings sent to root

### 1. Consume the original delivery OBSERVE before cancellation (definite)

The delivery loop around lines 6905–6915 calls observe then server->sync, and terminates when delivered becomes true. `production_observe_service` in gdextension_production_request.cpp:1167–1234 schedules asynchronously and consumes a returned cell only on a subsequent nonzero observe call. Consequently the successful-delivery iteration leaves the cell RETURNED. The subsequent record_schedule for output cancellation cannot accept a new RECORD over that original outstanding turn.

Use the same original-consumption pattern already present in the later owner-drain loop: after sync, consume a RETURNED original OBSERVE using observe; never set the control state directly.

### 2. Delivery alone is not the public CPU-copy readiness predicate (source-supported risk)

`production_copy` at request.cpp:1635–1708 additionally requires graph issue completion and every original native owner observation Complete. OBSERVE polls native sources before frame maintenance. Therefore exiting solely on source.delivered can precede the necessary observation turn. Drive bounded original observe/consume until actual public copy returns OK; Busy is legitimate continuation. Keep the final exact byte and issued/completed/returned assertions. Do not repair native observations or readiness flags.

### 3. Failure cleanup is intentionally fail-closed, not recoverable (limitation)

The fixture creates raw minimal MeshStorage/MaterialStorage owners and swaps singletons. Their successful cleanup is after all readback/cancel/release assertions. A REQUIRE failure bypasses it. More critically, ExternalLinkedOwnerTest::~ExternalLinkedOwnerTest in test_external_output_owner_state.cpp:57–68 CRASH_CONDs when actual created custody remains. Thus a runtime refusal during RED testing can end the subprocess with a crash after its first assertion; it does not demonstrate drain success. Preserve the first failure and run the RED test in its isolated process. Do not silence the assertion by clearing owners or writing RELEASED.

Before actual owner creation, a setup REQUIRE failure also bypasses raw mesh/material cleanup and singleton restoration. This follows the existing generation fixture pattern but is not robust failure-path isolation. A later scoped cleanup helper must obey actual lease ownership and may destroy CPU owners only after original generation release; a destructor that blindly frees them would create UAF risk.

## Initialization and capacity checks

- prepare_device precedes minimal MeshStorage construction, preserving ShaderRD singleton dependency ordering.
- Actual generation config: capacity 1, max_views/max_inflight 1, two actual mesh surfaces and two material selections; tags are real 32-byte buffers, unit bounds match acquire.
- Creation request reserves 1 resource/1 use/1 return cell. Producer reserves 4 resources/63 uses/4 return cells. Attachment sums are 5 resources/64 uses/5 return cells and 2 requests. Producer payload/readback capacities are exactly 4 bytes each, and attachment matches them.
- Six declared uses leave 57 dynamic use cells. Actual acquire requires action root + disposal group + seven storage members (9 spare), so the obvious spare-cell check is satisfied. Native quote 4096 and graph quote 16 are conservative; runtime membership computation still determines acceptance.
- Instance192 buffers are 192,192,32,64,32,40,512 bytes for two surfaces, within the bounded driver's 1024-byte per-allocation storage. One source plus seven owned resources fit its 16 IDs. Allocation/free count 8 is an unexecuted assertion.
- Upload DTO field order, READBACK producer ordinal 0, output-role extents and original source span were source-checked. Upload is actual retained-payload execution; no backing-memory expected-byte seeding.
- Both staging pools use original _insert_staging_block before allocation. Each has one 64-byte block, enough for the single 4-byte upload and download at required alignment. Download/upload IDs and bytes are distinct.
- Driver mode remains false for every other test. It is enabled only after generation metadata construction and before actual storage allocation.

## Success cleanup and state authority

On successful execution the order is actual public byte copy, original cancel, output-owner service, generation withdrawal and service, free_created, original source retirement/release, both request releases, staging unmap/free, CPU material/mesh destruction and singleton restoration, fixture cleanup. This is logically appropriate; runtime proof belongs to root.

The test never writes BOUND, alias witnesses, next_action, delivered, native observations or completion facts. Its assignment of frame handles/epoch and fake driver IDs is inherited minimal backend setup; it is not physical GPU or normal-engine Create qualification. Existing fixture.cleanup_owner suppresses native RD destructor because this is a partial CPU fixture; the explicit successful native drain assertions must remain meaningful and cannot be replaced by that suppression.

Public Instance192 remains gated. Current explicit runtime READBACK refusal must be replaced by reviewed original-custody admission before this candidate can pass. Negative witness/group/member/backing/usage and prefix/map-failure drains are outside this single success fixture.
