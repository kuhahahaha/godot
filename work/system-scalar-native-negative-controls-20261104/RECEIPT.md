# Controlled native-negative preparation

Engine base: 3da5cd3d18. This worker did not build an engine/DSO, run the host, or run a GPU workload. Source hooks, an engine regression test, and native negative driver/config are prepared for the root's one matching-host build and runtime session. Independent review by /root/b4_parser_review found no definite defect; review was source-only.

## Engine seams

All injection state, helper methods, environment reads and diagnostic hooks are behind TESTS_ENABLED. Qualification/non-System tickets are excluded. Variables must equal the exact string "1"; unset/other values leave the normal path intact. Each injection can fire at most once per original System ticket. No ABI, issuer, registry, completion fact, raw native error or free event is introduced.

- GODOT_SYSTEM_SCALAR_TEST_APPEND_UNKNOWN: only after the real checked dispatch returned Appended with a valid original graph handle. The test projects that adapter observation to Unknown/FAILED and clears its handle. The normal System finish/publish path must quarantine all original owners. SYS_SCALAR_INJECT logs the original graph tuple before the projection. This is an adapter-observation fault after real graph append, not evidence that the GPU/native driver returned an unknown issue. Recording alone is not submission/completion.
- GODOT_SYSTEM_SCALAR_TEST_SET_RETURN_ERROR: at the existing maintenance attempt seam, after compute has physically retired and while the original internal set has no free event, substitutes FAILED for that one maintenance attempt. The normal service then records the sticky first error before any subsequent set-return step. It does not manufacture a driver error or physical return. The set may already be enqueued; its ownership remains quarantined after the injection, including if a later actual free event arrives.
- Existing GODOT_QUALIFICATION_CAPACITY_TRACE=1 additionally logs the System graph/native admission inputs/result (SYS_SCALAR_CAPACITY). This diagnostic changes no capacity calculation or driver result. The 16KiB case uses the real worker profile and shared original driver bound. On a profile that legitimately accommodates it the negative must fail, never fake a refusal. The intended current host has enough real workers (>=6 with this Vulkan formula) for the combined graph/source bound to refuse the request.

## Native negative fixtures

Owned artifacts are negative_probe.cpp and negative_cases.json in:
/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/runtime

The runtime runner and positive/post-submit-close probes remain owned by /root/b4_guard_shader. It consumes schema1 config, clears inherited System TESTS variables before every process, sets only the case variables, and requires exact exit/markers and actual host selection/backend.

1. native_unknown: real checked append then observation projection. Requires Unknown/possible obligations, copy Busy with zero written bytes, repeated observe/cancel preserving Unknown, release and bridge close refused. Only after assertions and the explicit quarantined/process_isolation marker does the driver call _Exit(0). This intentional process isolation is not a native completion, resource retirement, normal DSO unload or automatic recovery.
2. native_fact_capacity (Vulkan): valid 16KiB backing/output request must produce the real SYS_SCALAR_CAPACITY refusal, Error ERR_PARAMETER_RANGE_ERROR/reason19, zero ticket/facts. A separate one-word request with nonce24 in the same bridge/backing must subsequently complete to 3.0 (0x40400000), physically release the internal set and original resources, and ACK/close normally. No fake driver/capacity injection is used.
3. set_retire_error: real compute/readback completion precedes the maintenance injection. observe returns sticky FAILED with the unchanged state8/revision/native_code report; same-revision copy must still return the original 3.0 word, and original native facts must show completion. Repeated reports remain byte-identical. release returns the same Error and bridge close remains Busy. The explicit quarantine marker then terminates the isolated process with _Exit(0), without clearing errors or fabricating ACK.

The unknown/set-error cases forbid normal DSO-unload/success markers and have retirement_claim=false. Capacity recovery has retirement_claim=true only if the original driver proves all completion/free/ACK checks. No timeout or arbitrary abnormal exit is accepted as a pass.

## Validation

- Engine syntax-only: system_scalar.cpp and qualification.cpp passed with TESTS_ENABLED both enabled and disabled; test_system_scalar.cpp passed with tests enabled.
- negative_probe.cpp passed -fsyntax-only for all three SYSTEM_NEGATIVE_KIND variants. The input SDK was the existing frozen generated header SHA256 ec25549317a591c3009de34f217bcb835f1d3cc52ba2d8782bde2af15d5dbad7, copied under syntax-only-inputs. It is strictly a syntax input, not a claim of matching the future rebuilt host. The runtime runner must use the caller's new matched SDK snapshot.
- check_test_hook_behavior.py runs the actual production TESTS helper bodies with scalar stand-ins: real-append prerequisite, valid handle, System-only, one-shot, compute-retired/set-present, existing-free-event exclusion, existing-error exclusion all passed (failures0). It does not prove real GPU behavior or concurrency.
- Existing Unknown/capacity and retirement-error source-body fixtures still pass. Engine doctests/native controls remain not-run.
- git diff --check passed. Full syntax command/results are in SYNTAX_RESULTS.json; CHANGED_FILES.json binds engine and runtime artifacts.

## Evidence correction

During native-driver syntax validation, the old retirement-error source fixture's stand-in GDExtensionDeviceSessionResultV1 was found to misname its third field native_code. The actual ABI is native_error/reason/expected/actual, with no native-code field. Production code had always correctly returned {Error,reason,0,0}; no production/ABI correction was needed. The fixture now names/checks the real expected/actual slots, continues separately checking the unchanged report.native_code, and its baseline red/working green evidence was regenerated (red9/green0). The earlier receipt's call-native-code wording was corrected. This correction does not create a native error observation.
