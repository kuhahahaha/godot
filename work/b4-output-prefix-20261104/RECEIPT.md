# Instance192 owned resource prefix — component implementation

Implemented the internal K-owned allocation/retirement-observation component in `ExternalOutputStorage`, plus seven engine doctests. This is **not** public acquire/seal, source/budget authorization, producer admission or `OutputReleased`. No production caller, new issuer, qualification ticket, token, arbitrary RID enrollment or source registration was added. Only existing `ParticlesStorage` and a TESTS-only friend can construct/call/destroy the class. Both constructors and destructor are private and copy is disabled.

The five changed source identities are locked in `SOURCE_SNAPSHOT.json` and enumerated by `OWNED_FILES.txt`. The only changes outside the three new files are one TESTS_ENABLED friend in each of RenderingDevice and NativeDisposalWitness; no new production RD port was introduced.

## Resource and lifecycle behavior

The component computes all seven owned extents before the first native create: current/previous records `192*N` each, metadata32, scratch64, status32, protected commands `20*S`, geometry proof `224+144*S`. It accepts only the existing Instance192 envelope N<=2^20 and 1<=S<=8, uses the existing checked byte-size helper for every extent, and keeps the sum in u64. The Core factory still decides actual native allocation success; RD exposes no storage-buffer byte-limit query here. The private command buffer requests the existing indirect usage bit, which Core maps to native INDIRECT usage. No CPU upload or GPU command is recorded by this component.

Capacity zero omits both record buffers entirely. It allocates the real control/proof resources required by the existing empty guard modes; no fake one-record buffer or exported writable descriptor is produced. All bytes remain uninitialized here: actual authorized producer/guard initialization is a later integration obligation.

One prefix object permits one allocation attempt. Every successful factory result retains its own original `NativeDisposalWitness`, RID and exact byte extent. A later create failure records the original creation error, closes future allocation, requests ordinary Core disposal for the completed prefix and retains each entry even if disposal is busy or uncertain. A caller cannot retry allocation into this object after failure, closure or return. It must retain the object through actual return; later independently authorized work would need a new object.

Before requesting retirement, the original owner must close future native uses and satisfy the existing recorded-use/retirement obligations for these resources. This component neither verifies nor manufactures that association. Retirement is explicit and idempotent. Busy Core state closes this prefix logically but does not pretend native disposal occurred; a later owner call retries disposal. A non-main RD or wrong RT rejects before changing that logical flag, including for an empty prefix. Observation is permitted during an active list because the existing Core observation port permits recovery observation; it does not append work. Reports distinguish LIVE, ENQUEUED, RETURNED and UNKNOWN per resource. Full-return is computed only from actual Core RETURNED reports for every created resource. An empty created prefix is vacuously returned, solely for destroying that empty component; this is never an output-release result.

UNKNOWN is sticky in the component. There is no production reset. Controlled destruction refuses live/enqueued entries and any prior unknown observation. Destruction itself does not free native objects or call maintenance. `maintenance()` delegates exactly once to the original Core maintenance port; there is no scheduler, N-frame rule, RS.sync guess or hidden polling loop.

The original ParticlesStorage owner must retain this object, its real budget charges, and the original K03 device-holder lease across the entire lifetime, including Retiring/Unknown. The stored RenderingDevice reference is not a device lease. The owner must never call observation/maintenance after device destruction. That owner association is a future integration obligation, not synthesized here.

## Deliberately absent joins

No borrowed source RID or existing shader/resource RID is accepted. No uniform set is created. The missing retained original source/operation/selected-root/budget association remains the dependency identified in `../b4-source-bridge-inventory-20261104/INVENTORY.md`. The owned-buffer factory cannot be used to enroll arbitrary resources. Actual surface/material/source/history pins, whole recorded-use retirement, uniform-set ownership, private guard recording, renderer adoption and physical owner ACK remain outside this component. The public six-method API and its positive acquire/seal path remain absent.

## Validation and limits

Three clang syntax checks passed: production component with TESTS_ENABLED, the engine test TU, and production component without TESTS_ENABLED. Exact commands, final source hashes and exits are in `SYNTAX_RESULTS.json`. `git diff --check` passed. An initial ambiguous BitField ternary compile error was corrected; the original diagnostic is retained separately and is not represented as a successful run.

The seven `[ExternalOutputPrefix]` engine doctests exercise actual RD creation/free/observation/pending-free functions with a fake native driver:

1. Third create fails: exact two-resource prefix retained and enqueued, original creation error preserved, no reallocation/destruction until actual queued driver calls return; stable RID/serial and exact driver IDs are checked.
2. Capacity0 omits record RIDs, allocates exactly five literal extents and keeps controls until return.
3. Invalid capacity/surface extents reject before any native creation.
4. Active-list busy retirement preserves all seven resources, retries idempotently and observes return through the original RD queue without a subsequent draw.
5. Backing-serial mismatch produces UNKNOWN, retains the prefix and refuses destruction/reallocation/retry clearing. Test-only teardown repairs the injected fault; there is no corresponding production API.
6. Closing an unallocated prefix and first-create failure both forbid reopening, with no hidden created resources.
7. Wrong device ownership and wrong RT refuse even empty-prefix close/destruction before mutation.

These **seven engine tests have not been executed**. No SCons build or new host was started. Their syntax success is not an engine test result, and their fake driver will not be native GPU proof when run. Successful real no-next-draw frame maintenance, native resource retirement, device fault/recovery and owner/K03 lifetime still need the appropriate actual runtime tests. The tests' explicit pending-queue drain only substitutes a ready retirement point in this fake-driver fixture.

A separate standalone check compiles the verbatim production class/body and checked-size helper with **mock RD/witness dependencies**:

```sh
python3 work/b4-output-prefix-20261104/run_controls.py
```

`CONTROL_RESULTS.json` records source hashes, compiler commands, run exits and expected assertion matches. Production controls pass. Three targeted mutants fail their intended assertions: omitting failure-prefix retirement, counting ENQUEUED as returned, and omitting sticky UNKNOWN. Additional controls cover maximum envelope literals, protected command usage, wrong RT, zero capacity, busy retry, exact mock identities and explicit maintenance delegation. This is control-flow/model evidence only; it does not execute real Core witnesses or driver queues. Generated `.cpp.in` inputs are retained; standalone executables are hashed then removed.

Independent review is being performed separately by `/root/b4_guard_wrapper_review`; its final result belongs to its own evidence package. This receipt does not pre-approve that review or any overall B4/source/renderer acceptance.
