# K01/K03 original core interface candidate

Status: revision3 in-review, source-informed candidate; no symbols below are delivered production APIs. This is the D-owned interface boundary for independent review, not acceptance of the full D package or an instruction to A/C to consume unstable bytes. Existing particle protocols remain the unique semantic authority.

Paired requirement: [core interface scope](FR_core_interface.md).

## 0. Decisions

The first independently buildable unit is the core identity/retention foundation. It does not certify an unload, create a completion token, or enable guarded mode until entry coverage, dual G2, G3 and host qualification are installed. K01 issue/completion and K03 guard/relay follow as separate implementations sharing this foundation. None depends on finishing all physics retirement branches. C owns original R01/R02 session/operation interpretation, A integrates bindings, D owns actual core identities and facts.

### Authority and ABI

Normative inputs: design repository `/Users/macbookair/Documents/Work/Dev/ANGEParticleDesign/docs/FR_particle_device_exit_PROTOCOL.md`, `contracts/device-exit/{OWNER_ADAPTER,ENTRY_SURFACES,EXIT_CALL_GRAPH,QUIET_WINDOW,ASSOCIATION_POLICY,RELEASE_COMPLETION,CORE_EVIDENCE_CAPSULE,BUILD_AND_HOST}.md`. Current root dispatch is `handoff/godot-fr/FOUR_SESSION_IMPLEMENTATION_PLAN.md`, 2026-10-22 production owner override. The public names in the protocol remain; the C spelling adds `GDExtensionDeviceExit` prefixes and uses pointers rather than C++ references.

ABI declarations belong in `core/extension/gdextension_interface.json`; existing SCsub generates the header/dump. Never hand-edit `gdextension_interface.gen.h`. ABI has no C++ containers, bool, references, size_t-sized fields, virtual objects or DSO allocators. Error is `int32_t` carrying the existing Godot Error numeric values. POD flags are uint32_t with unknown bits rejected. Structs start with `{uint32_t abi_version; uint32_t struct_size; uint64_t reserved;}`; exact v1 size/version/reserved are checked. Public struct layout is validated on each supported target, not assumed packed or used as wire bytes.

Capabilities are distinct C struct types each containing two uint64_t fields `{registry_instance, handle_serial}`. The fields are opaque, nonzero and not an API for reconstructing authority from an owner ID, RID, address or generation. Registry instance and serial never wrap or reuse within the process. Every operation checks the actual live core table, kind, owner/context, permission and retained reference. Threat model is cooperating native plugins, not memory isolation from malicious native code. Serialized pairs cannot authorize cross-process import; the original capsule admission owns that boundary.

Every successful retain issues a fresh reference handle to the SAME record with stable identity and mutex-consistent mutable observations. It does not just increment a count while returning identical bits: a bitwise copied handle denotes one reference and may be released once. This makes duplicate release detectable without stealing another observer's retain. The record identity/generation, ticket identity and one-use reference serial are distinct. Counter/quota refusal occurs before output publication; burned serials are never recycled. Last UINT64_MAX value is usable, then issuance is exhausted without addition overflow. Core work references are private and distinct from external reference handles.

### Concrete bootstrap declarations (candidate v1)

```c
typedef struct {
    uint32_t abi_version, struct_size;
    uint64_t reserved;
} GDExtensionDeviceExitAbi;
typedef struct { uint64_t registry_instance, handle_serial; }
    GDExtensionDeviceExitLibraryCapability;
typedef struct { uint64_t registry_instance, handle_serial; }
    GDExtensionDeviceExitLibraryViewRef;
typedef struct {
    GDExtensionDeviceExitAbi abi;
    uint64_t library_generation;
    uint32_t loader_kind; /* Dynamic1, Function2, Custom3 */
    uint32_t wrapper_association; /* Attached1, Detached2 */
    uint32_t loader_initialize_returned; /* 0 or1 */
    int32_t loader_initialize_error; /* valid iff returned==1 */
    uint32_t guarded_qualified; /* always0 in foundation */
    uint32_t reserved;
} GDExtensionDeviceExitLibrarySnapshot;

/* Only snapshot output has a caller-supplied ABI header.
   Writable, aligned capability outputs are zero on returned errors. */
int32_t device_exit_bootstrap_library_v1(
    const GDExtensionDeviceExitAbi *request,
    GDExtensionClassLibraryPtr library,
    GDExtensionDeviceExitLibraryCapability *out_library);
int32_t device_exit_retain_library_view_v1(
    const GDExtensionDeviceExitAbi *request,
    GDExtensionDeviceExitLibraryCapability library,
    GDExtensionDeviceExitLibraryViewRef *out_view);
int32_t device_exit_copy_library_snapshot_v1(
    GDExtensionDeviceExitLibraryViewRef view,
    GDExtensionDeviceExitLibrarySnapshot *out_snapshot);
int32_t device_exit_release_library_view_v1(
    const GDExtensionDeviceExitAbi *request,
    GDExtensionDeviceExitLibraryViewRef view);
```

### F01: refuse before OS open

Add base loader `virtual Error prepare_open(const String &p_path)` with a legacy default returning OK and `virtual uint32_t get_device_exit_guard_version() const` default0, `virtual LoaderKind get_loader_kind() const` default Custom. Kind values Dynamic1/Function2/Custom3. Custom loader cannot opt in through the default implementation; core refuses nonzero guard version unless the concrete trusted built-in kind supports that version. This is cooperative qualification, not protection against malicious core subclasses.

Dynamic `prepare_open` performs parse_gdextension_file, validates `configuration/device_exit_guard_version` as INT0/1, and freezes the resulting native configuration into the loader's pending-open state (path, library path, dependencies, entry symbol). Unknown/type-invalid returns ERR_INVALID_PARAMETER31. No OS open, dependency load or extension callback. FunctionLoader prepare validates libgodot path/function and captures its core-set guard version. Core calls prepare once before record reservation and any loader open. If required version1 is unavailable, return ERR_UNAVAILABLE2 before all loader opens. This entire foundation ships with guarded version1 unavailable.

Add `virtual void abort_prepared_open()` to base loader with legacy default no-op. Dynamic/Function built-ins implement only core pending configuration cleanup; no DSO callback/native close and no effect on an already-open association. Core `GDExtension::open_library` owns an RAII PreparedOpen scope immediately after successful prepare. Every return before `open_library` (feature unavailable2, record/issuer quota5, foreign/kind rejection) calls abort. On handing off to loader open, scope ownership is consumed; built-in open clears pending state on success/failure/wrong-path return via its own local guard. A prepare failure clears only that attempt's partial native configuration and leaves Empty. Calling prepare while already Prepared returns44 while preserving the previous prepared owner; its unsuccessful caller does not acquire abort ownership. Close clears leftover pending config only when owned by that same loader and no prepare operation is in progress; attempted reopen of active load rejects before prepare, preserving association.

Direct built-in open in Empty calls prepare then uses identical feature gate/abort transition; direct open in Prepared consumes only matching path, otherwise returns31 and aborts that prepared attempt. Core open always calls prepare, so it never steals a preexisting Prepared token. Test retries after quota5 and version1-unavailable2 use the same loader; after abort a changed0 config may prepare/open successfully. Version1 retry without config change remains2, never44. Immutable prepared configuration test rewrites file to1 after successful prepare0 then direct matching open; it consumes frozen0, not reparse. A new prepare later reads1 and rejects before OS.

Pre-OS test uses a real tiny Dynamic fixture whose platform constructor writes to a preopened marker fd/handle and entry writes a second marker. Its .gdextension specifies version1. Parent loads through manager and requires LOAD_STATUS_FAILED=1, zero marker bytes, no OS-open trace. A separately named direct GDExtension open case requires actual Error2; manager numeric2 means ALREADY_LOADED and is not accepted. Positive legacy fixture runs same library with0 and produces both markers. Paths `tools/device_exit/dso/foundation_fixture.cpp`, `tools/device_exit/dso/foundation_fixture.gdextension`, `tools/device_exit/dso/SConstruct` explicit source list, entry `device_exit_foundation_init`. This fixture returns initialize/deinitialize callbacks that only write POD markers and never enqueue work. Native constructor/destructor use platform APIs, no Godot interface before entry. No real GPU qualification claimed.

### F02: expose only observed foundation facts

The snapshot declaration above contains only observed foundation fields; no Closed, FailedClose, normal-admission, initialized-level, finalized or OS-close-success field is published.

Required layout: AbiHeader16, generation offset16, kind24, association28, returned32, error36, qualified40, reserved44, sizeof48. Actual layout compile checks establish target compatibility. Keep static_assert sizeof==48 and individual offsetof. No raw struct bytes serialized. No enum ABI layout assumptions.

The independent core record is the original future K03 record, with foundation facts only. `library_generation` published only after actual successful loader open; all identity reservations happen before open as F05. Wrapper association attaches when open succeeded, before initialize. Loader initialize outcome is written immediately AFTER the virtual initialize call returns, including missing entry symbol or entry false; it does not mean the entry function ran or that user initialization finished safely. No initialized-level inference from level_initialized. A wrapper association is detached after its existing close call returns or at destructor tail if no close call was needed, BEFORE releasing its record ref. This says only that this wrapper no longer owns that load association. It never says the DSO was unloaded or safe to call. Dynamic void close's missing OS Error is intentionally not exposed as OK. FunctionLoader also reports Detached, with kind Function; no OS claim.

Transition table: prepare/unsupported/reservation/open failure→no external capability; open success→Attached/returned0; initialize return(any Error)→Attached/returned1/exact Error; failure cleanup close return→Detached retaining the preceding Error; normal explicit/reload close return→Detached; destructor after prior close→idempotent Detached; repeated open while open→44/no new identity; successful reopen after close→new generation/new record while historical view unchanged. Custom kind always unqualified and its outcome is exactly virtual method return only. Legacy method behavior/error logging remains existing; foundation does not fix close failure or claim G1/G2/G3. Full K03 later adds observed guarded lifecycle records, typed close and validity fields through a separately frozen ABI revision/API, not repurposing this snapshot's bits.

### F03/F06: minimal observation permission, no missing worker registration

Foundation metadata views are explicit transferable observation capabilities. They authorize ONLY core snapshot observation, clone and release for their exact record. They require no ambient thread registration or full K03 entry permit. They cannot register callbacks, enter DSO, mutate record, emit evidence grants or request device work. This is intentionally narrower than guarded normal/cleanup registration, whose real worker permits remain later K03. It eliminates an unimplemented observer registry and does not treat ThreadID as authority.

Bootstrap library capability is borrowed and may be acquired only within exact actual `BootstrapScope` on loader thread, with nested scopes pushed/restored. `retain_library_view` also requires that matching live scope. After scope exits no new authority→view conversion is allowed. Callers needing a lasting metadata view create it before return. Add:

```c
int32_t device_exit_clone_library_view_v1(
    const GDExtensionDeviceExitAbi *request,
    GDExtensionDeviceExitLibraryViewRef source,
    GDExtensionDeviceExitLibraryViewRef *out_view);
```

In THREADS_ENABLED builds, clone from any existing valid view is legal on any thread while registry Running, including a Detached historical record. Clone issues a fresh one-use handle to same record. Passing bits to another observer transfers a single ref; concurrent cloning/releasing same source is serialized under mutex: clone-before-release succeeds with independent ref, release-before-clone fails30. Neither transfers library/DSO authority. Copy/release are legal from any thread through Finalizing; clone/new bootstrap issuance refuses44 in Finalizing. Revoking bootstrap authority at scope exit doesn't revoke existing metadata views. A guessed owner number, different-kind handle or released source fails30. No view APIs ever call DSO/error logger or consult Object/thread local state. THREADS_ENABLED-disabled builds require their existing single-thread caller contract; the no-op mutex does not promise cross-thread safety. ABI pointer validity is ordinary native C caller contract; null/misalignment checked, arbitrary unreadable pointers cannot be made safe by this interface.

Private `BootstrapScope(Registry &, RecordRef, GDExtension *expected)` can only be constructed by friend GDExtension loader path or test friend; Scope has previous TLS pointer, core record ref and expected pointer. It is stack-lived and main/loader-thread only. `bootstrap_library` compares pointer equality without dereferencing unknown pointer, validates active scope's actual record. Private core work refs are C++ move-only `RecordRef`; no public arbitrary core-context grant. F03 wrong-owner-same-thread test uses library A pointer while B bootstrap scope is current; zero authority output/error30. Snapshot sharing does not require any owner registry side effect.

### F04/F05: storage, issuance, finalization

One process-lifetime `RegistryStorage` owns fixed native arrays, a mutex and counters; no Object/RefCounted/Variant/Callable/function pointer/DSO deleter. Use aligned static byte storage with placement construction through C++ thread-safe local static once; intentionally never destroy storage/mutex at normal engine teardown, so saved metadata function pointers never touch destroyed Godot services. Construction uses fixed arrays and core BinaryMutex, not Godot allocation, logs or Thread state. BinaryMutex already selects std/Mingw threading or the thread-disabled implementation; its process-lifetime wrapper is never destroyed. This is bounded process-lifetime core metadata, not a leak per reload.

Defaults:1024 record slots;8192 external view slots; up to4096 private references per record. One64-byte fixed failure detail per record plus one64-byte registry reservation failure slot. These are internal foundation quotas, not semantic limits for full guard/native/capsule packages; extending them later requires reviewed capacity configuration before any registry issuance. Test constructor uses smaller fixed logical limits over same backing arrays, never larger; production has no environment/script quota override. Record slots contain only the snapshot fields, identity/counters and fixed failure detail. External ref slot stores handle serial/kind/record index; no heap, HashMap or arbitrary string. Linear table search is bounded8192 and chosen for first slice; later optimization must preserve IDs/semantics. No allocation under or outside the operation after static initialization. Changing logical limits is allowed only for isolated test Registry with no records/refs.

Separate process-monotonic uint64 issuers: registry_instance (per init cycle), library_generation (per successful or burned reservation), reference serial (shared kind-disjoint authority/view handles). All start1; reserve checked next values under mutex BEFORE open; max may be reserved exactly once then exhausted flag, never ++max. Registry identity changes per same-process initialize cycle; library/reference counters do not reset. Test private instances have explicit separate registry instances issued from same process issuer, so equal serial values never authorize cross-instance. Test friend may set counters near max only on an otherwise empty isolated test storage, never production singleton. Diagnostic/source identity is not globally unique across launches; cross-process capsule handles are separate.

Reserve before open: free record slot, one borrowed bootstrap authority serial, generation serial, one wrapper private ref, one reserved BootstrapScope private ref,64-byte fixed failure cell. Counter exhaustion or logical quota error5, no OS call. After reservation, even open failure burns IDs. Slot released without allocating; unused authority becomes invalid. No view slot reserved until requested from bootstrap; its failure5 returns no view and changes no existing refs. Clone reserves free view slot+serial before incrementing the record's external count or output. Both commit under same mutex. Private retain checks4096 and record liveness before increment; public callers cannot synthesize it. Required private cleanup holds are reserved by initiating operation, not acquired during teardown after quota failure. Record reclaim requires detached wrapper, no bootstrap scope, zero private refs, zero external refs. This foundation has no accepted native/pending-close/evidence refs yet; later participants must use these actual private refs before their publication.

Running→Finalizing happens for the exact current-host token before the interface map is cleared: block new issuance/clone/open reservations, keep reads/releases and internal detached observations. No wait or spin for observers. Reclamation may transition Finalized only when `engine_cycle_ended && no occupied records/refs/scopes`; empty storage before manager destruction is insufficient. Saved metadata functions after Finalized return30 for stale handles (ABI errors31 take precedence). Already admitted operations hold mutex/private ref only for bounded metadata work, never wait on users. Storage persists until process exit.

`GDExtension::initialize_gdextensions() -> Error` begins the registry cycle and registers the same interface map only on OK. `register_core_extensions() -> Error` preserves returned Error and never manager-loads after failure. In normal `Main::setup`, use its actual `exit_err = register_core_extensions(); if (exit_err != OK) goto error;`; existing error label returns exit_err, not a new unrelated local.

`Main::test_setup` returns the registry Error before register_server_types. Its REAL caller `Main::test_entrypoint` stores the result. On error it MUST NOT invoke test_main or full test_cleanup. Selected policy is explicit process termination for this incomplete test host, not invented partial cleanup: write a fixed core C stdio diagnostic `DEVICE_EXIT_TEST_SETUP_FAILED error=<int>` to stderr, flush that stream and call `std::_Exit(static_cast<int>(setup_error))`. Error44/5 are nonzero portable process exit values. This branch is TESTS_ENABLED host infrastructure; it deliberately performs zero half-initialized service teardown and does not claim graceful cleanup or device/library closure. No guarded DSO load has run. Test setup's other existing error returns use the same caller handling. A blocked stdio sink is bounded by parent watchdog; output transport is not a completion proof. Normal production setup still uses existing Error unwind, not this test-only stop policy.

The two real entrypoint failure cases are isolated children whose parent drains stderr/stdout and checks exact marker/error/process exit plus zero doctest-entry and zero extension-entry markers. Private registry unit cases alone cannot satisfy them. A TESTS-only source seam controls begin-cycle to fail44/5 before any extension is loaded, selected by an explicit fixture option and child identity; it does not synthesize a successful native fact. This seam is compiled out in production module-disabled builds. There is no new runtime environment switch that can disable safety checks.

Do not claim full Main supports multiple same-process start/stop sequences. Required issuer restart tests use an isolated minimal actual core lifecycle harness invoking the original registry startup/map-clear/post-manager hooks. Whole Main test/setup failure cases each start in a fresh process. Reset `_start_success=false` at actual test_setup and normal setup entry so those failure paths cannot inherit a prior success flag, but this does not certify full Main repetition.

Core startup fields: `_is_core_extensions_registered=false`, `_extension_teardown_in_progress=false`, `_active_extension_cycle=0`, `_finalizing_extension_cycle=0`. At register_core_types for a new nonconcurrent host, require no active/finalizing CURRENT-HOST token and initialize flag false; old detached registry records may still be Finalizing with engine_cycle_ended=true and no current-host token. At each attempted register_core_extensions ensure flag false and active0 before begin-cycle. On success set active token immediately after registry creation, before any load, then set registered flag after CORE initialization completes. If already registered/active in current host, return44 without clearing its ownership; this is repeated registration, not new host initialization.

`unregister_core_extensions`: reject nested work by returning immediately when `_extension_teardown_in_progress` is true; otherwise set that private flag before any manager callback. If registered flag true, consume it to false BEFORE manager deinit. Only after that deinit returns, take active token into finalizing token and clear active0; call begin_finalizing(exact token) and clear interface map once. A recursive unregister during deinit cannot clear the map beneath the current callback or take its token. Clear teardown-in-progress after the outer operation returns. If no active token, do not begin_finalizing or clear a different retained cycle; duplicate unregister after successful transfer is an idempotent no-op. Keep finalizing token until after `memdelete(gdextension_manager)` in unregister_core_types, then take/clear that token and call finish_engine_cycle. It records engine_cycle_ended=true. Finalized requires this flag AND zero occupied records/refs/scopes. Direct recursive full `unregister_core_types` from an extension callback is not a supported host API; this foundation does not introduce reentrant whole-engine teardown.

Cycle-token operations are private core C++ calls, not new GDExtension exports: `Error GDExtension::initialize_gdextensions(uint64_t &r_cycle)` zeroes r_cycle before begin and sets it only on success; `void GDExtension::finalize_gdextensions(uint64_t p_cycle)` calls `Registry::begin_finalizing(p_cycle)` and clears the map only on its first successful transition; `void Registry::finish_engine_cycle(uint64_t p_cycle)` is invoked after manager deletion. `Error register_core_extensions()` guards active/finalizing/teardown state before beginning and captures r_cycle in current-host active state before manager loads. Exact mismatching tokens are ignored without modifying a retained cycle; duplicate finalization cannot clear a later cycle's map. Tests observe actual hook counts, not a fabricated close-success boolean.

Failed cycle B gets no token, cannot touch retained A. Sequence: A's manager destroyed/finish hook consumed; A historical view keeps registry Finalizing; B begin returns44, B unregister has flag false/tokens0, does not deinit/finalize A; release A last view finalizes registry; C begin gets a fresh instance and monotonic generation/handle counters. Exact hook counts and fresh identities are asserted in minimal lifecycle harness. No hidden wait for observers; process storage remains bounded.

No capacity restoration reuses identity: slots recycle, serials do not. Failed repeated opens can exhaust counters but cannot leak slots. Duplicate release never decrements another ref. No unknown release logs from callback-capable Godot machinery. All rejected operations leave records/counters unchanged except explicitly burned pre-open reservations; this exception is recorded in tests, not falsely called total rollback.

### Reference accounting and output validation

Pre-open wrapper reservation owns private count1. Reserve a second BootstrapScope private ref before OS open (so4096 cannot fail after opening); enter scope consumes that reserved hold, exit restores TLS and revokes borrowed authority/scope-presence BEFORE dropping it/rechecking reclamation. Open failure cancels both unused holds and frees provisional slot; counters remain burned. After successful open/entry scope return, wrapper holds1; close-return detaches and drops it exactly once, destructor does not drop twice. Snapshot operations run under mutex, require no extra private ref. Stable record identity does not mean mutable observation fields never change: one snapshot copy is consistent under lock.

Only snapshot output contains caller AbiHeader; capability outputs are plain two-u64 handles. Check nonnull/alignment before reading/zeroing. On safely writable capability output, all returned failures zero it; invalid null/misaligned pointer cannot be written. Snapshot validates input header/reserved then on success writes complete48-byte POD, preserving declared header values and reserved0. Before initialize return, loader_initialize_error is0 but valid flag0, so never a successful initialize claim. Any snapshot error leaves entire caller buffer unchanged. No write beyond v1 size; aliasing request/output or input view memory is unsupported C caller contract and documented. All new symbols stay optional and unavailable until actual implementation registration.

Foundation dynamic fixtures use the current generated header and normal native host. Ordinary doctest uses real FunctionLoader for bootstrap/generation branches but does not mutate process-global registry startup. Phase/reclaim unit cases use isolated Registry storage. Whole-cycle cases use an isolated minimal real core lifecycle harness, not a claim that all Main state supports repetition. Native and startup children must terminate with exact expected observations. SCU/non-SCU, module-disabled and release validation remain required.

### K01/R01 integration boundary (not yet a frozen ABI)

| Object | Actual issuer / attachment | Required proof / retention |
| --- | --- | --- |
| DeviceEpoch | core RD registry at successful initialize, before external device publication; local RD creation has its own epoch | no epoch on failed initialize; loss closes new work but retains old epoch; actual recreated RD gets new epoch |
| Original session | C/R01 original session registration using a D-issued device registry context | session starts1, duplicate same association returns original key; D never allocates a replacement R01 ledger |
| AttachmentCapability | D registration of authenticated A/C original owner association, exact library+RD+session | requires original typed resolver grant, not a caller struct of owner numbers; receipt/callback/lease retention remains exact |
| CutoffCapability | RT snapshots existing registered tokens under enqueue lock | immutable exact token set across all slots; no count hint, unrelated/late entry excluded |
| QuietTicketCapability | Main-controlled admission request cell | command refs survive observer release/timeout; stop ack/source lifetime/sync caller lifetime separate |
| TicketCapability | begin_owner_drain after validated Quiet and cutoff | one original attempt continuation, no replacement world obligation; record survives command and owner adoption |
| ReportCellCapability | core immutable native/retirement observation | retain produces independent one-use reference; last release after span consumer; no DSO payload |
| CompletionCapability | actual driver native operation and original unit registration | every issued unit, transfer and Metal hidden commit covered; query cannot mint Completed from Error==OK |
| RelayToken | K03 adopted real callback holder cell | cancellation != underlying holder destructor return; core-only frame payload and byte mailbox |

Full signatures for the original session/owner grant import are NOT available from current Godot source. The current normative OWNER_ADAPTER ports name private owner refs, not their implementation. Resolve with A/C using the exact original source contract; do not implement a registration overload accepting naked IDs to make tests pass. This does not block implementing D's native driver observation records or core library identity foundation. It DOES block claiming successful cross-owner attachment/record consumption.

Protocol `begin_owner_drain`, `poll_owner_drain`, `request_quiet`, `observe_quiet`, retain/release_ticket/report and relay names retain their signatures/roles. New reference handles are per retain; immutable spans belong to the corresponding report reference. In ABI, polling copies into caller outputs synchronously, but asynchronous commands hold only core cells; output pointers never cross queue/thread. A nonzero poll posts/coalesces bounded control and returns the latest immutable Pending/progress report, never waits for RT. A zero-limit poll is a pure snapshot and cannot enqueue, consume receipts or recycle. Exact coalescing/result-revision fields need the full native report candidate before implementation; foundation does not advertise these functions.

### Full package dependency map

Foundation → independent core record/grants → K03 entry/cohort/holder gates + binding dual-G2 → all exit routes/G3/capsule closure.

Foundation + original A/C grants → K01 driver issue/completion + exact disposal → main-RD Quiet/restoration + R02 checked append/upload → original R01/E09 receipt consumption.

C4 cleanup tickets/Active Reload and shutdown remain separate D branches joining the actual exit/holder proof. Physics query teardown shares the accepted queue baseline and supplies its actual shutdown safety, but its full matrix is not an artificial prerequisite for source/API work on the two chains above. No chain is accepted by a CPU model or TESTS capture bool.

## Sources

Current source HEAD cbcbecf262b2bfe349ca9cf2f4dc11c1c9329f83 with preserved dirty tree; source hashes accompany review. Observed facts:

- [Open/initialize/close](../../../core/extension/gdextension.cpp:804): global lookup passed to initialize; close void; level decremented before callback; destructor closes. Independent record absent.
- [Dynamic close](../../../core/extension/gdextension_library_loader.cpp:245): OS Error discarded and handle nulled unconditionally.
- [FunctionLoader](../../../core/extension/gdextension_function_loader.cpp:45): same entry signature; close only clears pointers/path, no OS close.
- [Base loader](../../../core/extension/gdextension_loader.h:40): pure virtual close void, no guarded metadata.
- [Interface generator registration](../../../core/extension/SCsub:15): JSON generates header/dump; cpp glob includes new core implementation.
- [Callable holder destructor](../../../core/extension/gdextension_interface.cpp:230): actual free callback return is currently untracked.
- [Core startup/finalize](../../../core/register_core_types.cpp:392): map initialize before library load, CORE deinit then interface-map clear; registry must outlive this if pending records exist.
- [RD initialization](../../../servers/rendering/rendering_device.cpp:8645) and [local creation](../../../servers/rendering/rendering_device.cpp:9364): actual device creation path; no proposed epoch/capability is implemented today.
- [Test discovery](../../../tests/SCsub:10): recursive cpp plus force-link; SCU separate list still must include new object-test TU.

Quiet baseline is accepted repaired `core/templates/command_queue_mt.h` SHA256 `9d92cd8588434248d21afa48b7e13f8fc5af0e1600ebc34f5fc984cc4c2403fe`, queue review PASS_BOUNDED_IMPLEMENTATION and 8/82 tests. Root confirmed this baseline after the old QUIET_WINDOW read-only wording was raised; formal contract successor/pin review remains pending. Quiet adds no queue scheduling/cancel/relocation/replay/early ack. Do not claim old source byte identity or full Quiet acceptance.

## Implementation

No production edits until independent rereview of revision3, its tests and exact source manifest. Execute directly in current D tree, preserving all dirty prior work, one builder.

1. Freeze private identity table/retention definitions and ABI declaration subset. New `core/extension/gdextension_library_exit_guard.{h,cpp}` owns record types, registry and metadata operations; this is the same planned K03 store, not an additional registry. JSON declaration changes go with actual implementations; unimplemented functions are not registered. Add separate test TU `tests/core/object/test_device_exit_contract.cpp`, force-link, source discovery/SCU checks.
2. RED tests of actual registry/reference operations: wrong-context lookup, nested bootstrap, duplicate release, max-counter issuance, failed reservation, retained core snapshot after source wrapper destruction. Missing symbols are compile evidence, not semantic RED; once seam exists use invalid pre-repair behavior and targeted mutations. Core test helper may set near-exhaustion before issuance but cannot synthesize accepted native facts.
3. Implement metadata registry, single-use view refs, bootstrap scope stack and strict POD copy. No production guarded advertisement yet. Exercise a real FunctionLoader entry to prove pointer/scope registration; dynamic entry test later must verify actual new load identity. Tests cannot infer OS unload from FunctionLoader.
4. Generate header/dump; C and C++ smoke compile declarations; check layout with platform-specific sizeof/offsetof assertions, regenerated dump equality and old ABI unchanged. Run CPU suite with module disabled to demonstrate production ownership; repeat SCU discovery, release compile where validation must survive NDEBUG.
5. Independent implementation review and targeted negative controls; freeze source/host/commands. Only then offer exact delivered foundation version to A/C, explicitly listing no guard/native qualification. Continue complete guard/driver/Quiet/relay work and all original matrices.

Success check: authenticated original library record, deterministic validation, retained observation and no dangling DSO fields demonstrated through real core code. No NativeProgress/CanClose/GPU success is emitted by this slice.

## Tests

Candidate test names below are NEW, not currently discovered. The `device_exit::foundation/*` selector must match the complete implemented CPU name set from T01–T22, with separately enumerated native/startup child modes; do not claim twelve total cases. Existing runner path is `work/skin-lifecycle-integration/evidence/run_epoch17.py`; it freezes each host and inputs and rejects zero matches. Dedicated foundation runner successor must allow SCU/module-disabled/release configurations without rewriting historical receipts.

| ID | Trigger | Expected | Test / command | RED |
| --- | --- | --- | --- | --- |
| T01 | ABI size/version/reserved/null/misaligned output |31 before mutation; writable capability zero, invalid pointer no-write, snapshot unchanged | foundation/abi; CPU selector above | accept unknown version |
| T02 | exact FunctionLoader entry vs saved foreign pointer | only current bootstrap resolves; no arbitrary dereference | foundation/bootstrap; same | infer owner from pointer alone |
| T03 | nested loader then return, worker call without scope | nested record differs, outer restored, worker refuses | foundation/nested-scope; same | single non-stacked TLS |
| T04 | retained view, copy bits, release twice | second release30, other retain stays usable | foundation/reference-identity; same | same handle for each retain |
| T05 | unknown/foreign registry, kind substitution | stale30, no mutation or state leak | foundation/context; same | compare serial only |
| T06 | max serial/generation/registry counters | max once then5, old views still work | foundation/exhaustion; same | wrap to1 |
| T07 | record/reference quota fails before issuance | no partial authority, exact preexisting refs preserved | foundation/reservation; same | publish before reserve |
| T08 | release final observer while private work ref exists | record lives until actual core ref return | foundation/core-retention; same | erase at observer0 |
| T09 | wrapper destroyed, core record/view survives | snapshot no Object/DSO dereference; no Ref resurrection | foundation/detached-view; same | retain raw GDExtension* |
| T10 | opt-in1 before complete hooks; legacy absent | guarded unavailable before entry, legacy unchanged/unqualified | foundation/feature-gate; real FunctionLoader | advertise qualified at registration |
| T11 | failing open/entry, repeated open, same image reload | no fabricated init; distinct successful generations; old retained view unchanged | foundation/generation; fixture input pinned | reset issuer on reload |
| T12 | concurrent snapshot/retain/release | consistent snapshot, no lock over callback/destructor, once reclamation | foundation/concurrency; same | reclaim after unlocked raw lookup |

Foundation unit selector remains `device_exit::foundation/*` but exact discovery is derived from this table and T01–T12; no claim of twelve total tests after additions. Native/startup rows run isolated named child modes, not in shared doctest process. New runner `tools/device_exit/run_foundation.py` is a planned implementation artifact; it records executable/DSO/config/input SHA, command, exit, markers and exact case set. Existing epoch runner can build/freeze CPU hosts but does not itself validate these child modes.

| ID | Trigger | Expected | Test / command | RED |
| --- | --- | --- | --- | --- |
| T13 | Dynamic guarded1 vs legacy0 | manager LOAD_STATUS_FAILED=1 for1, zero initializer/entry; separately direct open Error2; legacy control runs both markers | `python3 tools/device_exit/run_foundation.py --case pre-os-gate` | move feature gate after OS open; exact initializer marker fails |
| T14 | init false/missing-symbol/Function detach/custom close | returned/error reflect actual virtual initialize; Detached only association; qualified0 | CPU `foundation/observed-fields` plus runner `--case dynamic-init-failure` | infer initialized or OS close from wrapper state |
| T15 | only view shared across threads; source clone/release race | clone-first independent handle survives; release-first30; Finalizing clone44, copy/release legal | CPU `foundation/view-transfer` | same handle on clone or ambient ThreadID authority |
| T16 | map clear then wrapper close/destructor then last view | no record loss, no callback from copy, finalizing until engine ended+last ref | CPU `foundation/map-clear-retention`; minimal core harness child | reclaim at map clear |
| T17 | private work ref survives wrapper/view | record retained, restart44 until private drop, then fresh cycle | CPU `foundation/private-retention` | count observers only |
| T18 | each record/view/generation/authority reservation failure |5 before OS/publish; exact old refs; repeated failure restores slots; same loader retry | CPU `foundation/reservation-boundaries`; runner `--case prepared-abort` | leave Prepared or publish before reserve |
| T19 | A retained→B refused→release A→C success; duplicate hooks and unregister reentry inside deinit | no B deinit of A, no map clear under deinit; exact begin/end counts, new registry/serials; full Main repeat not claimed | CPU `foundation/cycle-ownership`; minimal core harness child | stale static flag/token or empty-only Finalized |
| T20 | config rewritten after prepare, wrong-path open | immutable prepared config; wrong-path31 then retry; active reopen44 preserved | runner `--case prepared-config` | reparse after feature check |
| T21 | test_entrypoint registry begin fails44 or5 | exact stderr and process exit44/5, zero doctest body, zero full test_cleanup, zero DSO entry | runner `--case test-startup-failure` with two variants | ignore test_setup Error or invoke full cleanup |
| T22 | normal Main::setup begin fails44 or5 | setup returns exact exit_err; original error unwind once, no extension load | runner `--case normal-startup-failure`; main entry return observer | return default31 or load despite failure |

T13 manager and direct tests are separately named invocations; no manager numeric2 acceptance. Parent may use existing manager failed status plus actual lower-boundary source event with2, but a fixture-populated side flag cannot replace the real error return. T21 `_Exit` bypasses teardown by design; T22 asserts existing actual unwind, not T21 policy. Targeted negative controls intercept pre-danger before partial-host test dispatch or DSO initializer for hazardous variants, plus a separate healthy child; random crash/timeout is never positive detection.

Tests of complete native behavior remain original CaseSpec59/230, capsule CP01–CP29 plus current original-source successors, all route/cohort and P01/C4/Active Reload/physics neighbors. Foundation tests do not replace them.

## Example Impact

Optional `.gdextension` metadata and ABI need generator/bindings documentation through A, with unavailable guard version error before entry. No new GDScript API or gameplay example in foundation. Legacy extensions remain unqualified with existing ABI. Real DSO, native callback holder, original R01 session and E09 consumers require later integration tests; no fake Ready examples.

## Build and acceptance

Existing macOS command uses `/Users/macbookair/.local/bin/scons platform=macos arch=arm64 target=editor tests=yes module_skinned_mesh_capture_tests_enabled=yes -j14` via runner; epoch19 D ceiling15. Source/host frozen throughout run, no parallel builders or source edits. First foundation checks need actual new declarations/test discovery, not rerunning unchanged queue suite as proof. Dynamic runtime requires BUILD_AND_HOST normal rendering/DSO host; CPU `--headless --test` qualifies metadata only. Vulkan/D3D12/Metal/Metal3 native and template release gates remain unrun and mandatory in their applicable lanes.

## File-change checklist

| File | Planned action / responsibility |
| --- | --- |
| core/extension/gdextension_library_exit_guard.h/.cpp | new original K03 registry, core record, private refs and candidate bootstrap operations |
| core/extension/gdextension_interface.json / gdextension_interface.cpp | declarations/generated ABI registration; separate guarded lookup later |
| core/extension/gdextension.cpp/.h | record lifecycle and exact loader bootstrap integration |
| core/extension/gdextension_loader.h | prepare/abort-prepared-open, safe default guard-version/kind; later typed close contract |
| core/extension/gdextension_library_loader.cpp/.h | frozen prepared config, pre-OS gate, abort/consume ownership; later typed close |
| core/extension/gdextension_function_loader.cpp/.h | pre-open core opt-in and real bootstrap test; LogicalDetached only |
| core/register_core_types.cpp/.h | Error startup result, exact current-host flags/tokens, idempotent finalize and post-manager finish |
| main/main.cpp | normal exit_err propagation, test_setup result check and explicit test-host fatal branch; reset _start_success |
| tools/device_exit/run_foundation.py, tools/device_exit/dso/foundation_fixture.cpp, tools/device_exit/dso/foundation_fixture.gdextension, tools/device_exit/dso/SConstruct | new isolated native/startup runner, real constructor/entry markers and pinned child artifacts |
| tests/core/object/test_device_exit_contract.cpp | new CPU TU, force-link and exact foundation discovery |
| core/extension/SCsub, tests/SCsub, scu_builders.py | verify existing discovery; modify only if required by real build |
| external godot-cpp interface/termination bridge | A owns integration after D frozen interface; not edited here |
| servers/rendering and drivers | subsequent K01 exact native implementation; no foundation success substitution |

## Review

v1 review is preserved in INDEPENDENT_INTERFACE_REVIEW.v1.md. Revision2 review INDEPENDENT_INTERFACE_REREVIEW.md accepted the core design directions but found R2-01–R2-04 startup/token/abort/oracle gaps. Revision3 integrates their exact closure into the canonical text and T13–T22; FOUNDATION_REVISION3_INTEGRATION.md records rationale, not an alternate contract. Revision2 PLAN/FR and manifest are preserved. Fresh nonauthor rereview and mechanical check pending; no production implementation accepted by this document. Full guard action grants and R01/A original owner import remain later interfaces, not executable complete K01/K03.

## Self-review

Feasible: source paths and generator/base loader checked. Complete: foundation candidate covers negative identity/retention branches, full package tracked separately. Integrated: original registry/loader/JSON and actual A/C ownership retained. No overlap: original session/obligation ledger not copied. Testable: T01–T22 source-driven branches, mutations, actual loader and startup callers; native matrix preserved. Independently executable: NOT YET, reviewer must close the revision3 startup/token/abort/oracle fixes before production implementation.

## Delivery gate reconciliation — 2026-10-24

Closeout evidence is source/static only. Accept the P01 cleanup (`8cf518b407`) and E25 rollover helper (`77e0a73bfc`) within their scoped source boundaries, plus the E15/E16 retirement static audit. Do not mark acceptance, runtime credit, native qualification, GPU qualification, or production readiness true. The recorded full-build blocker is the macOS archive failure `No space left on device`; no further full-build retry is required until capacity is restored.
