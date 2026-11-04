# B4 Core native disposal mechanical port

Base: `8355b9a7ae2fb5e0ceaaa8453021865df16add62`. Implementation in the isolated `b4-renderer-continuation-20261104` tree only. No engine build, engine test execution, GPU execution, System retest, or frozen a2 tree mutation occurred.

## Implemented boundary

`RenderingDevice` privately friends the existing Core owner selected for this continuation, `RendererRD::ExternalOutputStorage`. It provides typed create/free/observe and maintenance methods. `NativeDisposalWitness` is default-empty; callers cannot fill its private refcounted cell or create authority from a raw RID, bool, string or qualification ticket.

Supported factories: new storage buffers without an initial upload, and persistent uniform sets consisting only of these buffers. Storage factory reserves its observation cell, exact RID, original backing serial and mutable graph tracker before native allocation. Uniform-set factory reserves its cell/serial and exact RID before its native descriptor allocation. Output arguments are published together only after successful creation. Failed native allocation releases only the unused reservation; no witness or completion is fabricated. Unsupported uniform types, foreign/untracked/qualification buffers, missing mutable trackers and transfer-worker association are rejected before invoking the original descriptor factory. The tracker precondition makes `_buffer_make_mutable` return false, and the worker precondition avoids implicit transfer synchronization.

Buffer and uniform-set structures retain the cell, and original pending-free copies retain it after logical RID removal. The original `_free_pending_resources` driver calls remain the only place that invokes native-return publication. Each publication matches the original RD ObjectID, RID, original backing/object serial and native driver ID. Wrong identity or invalid transition makes the cell UNKNOWN; a subsequent correct return cannot repair it. Driver callbacks see ENQUEUED; RETURNED becomes observable only after the actual free call returns. No DSO callback, pointer to K storage, global registry or business issuer was added.

The Core owner is still responsible for closing all operation/lease obligations before requesting free. This port cannot authorize an E09 operation, establish a source association, clear an Unknown operation, or certify aggregate OutputReleased. Public/ordinary RD resources retain their existing behavior. VA/IA, texture, sampler, shader and pipeline disposal are unsupported by this port; shader layout input to a set is not shader-retirement evidence. External E07 source buffers cannot enter this restricted descriptor factory.

`_qualification_maintenance` delegates to the same RD maintenance implementation under its new mechanical name. Its frame/fence checks and progress semantics are unchanged; a new empty/invalid frame-array preflight rejects an unconfigured Core caller before frame-index arithmetic. Registry admission, qualification resource-use checks, free enqueue policy and original owner acknowledgement remain in their original paths. The Core entry advances physical queues only; it does not issue qualification ACK or bypass closed-use checks. A frame number, elapsed N frames, logical free and RS.sync do not publish RETURNED.

## Verification

- `check_disposal_behavior.py red`: executes original base return-site loops against deterministic driver/queue stand-ins; 2 failures because the original loops provide no Core completion observation.
- `check_disposal_behavior.py green`: executes current actual return-site loops, actual cell identity/transition methods and actual publication method; 0 failures. Includes observer-drop metadata retention through the native call, independent buffer/set results, identity mismatch and Unknown persistence.
- `check_disposal_behavior.py premature`: moves publication before driver calls; 3 failures. This is a deliberate ordering mutation, not a driver/GPU failure.
- `test_native_disposal.cpp` contains four force-linked engine tests using the original RD factories, dependency cascade, pending-free queue and deterministic driver. Checks driver callbacks see ENQUEUED and live metadata, logical free does not return completion, unsupported/transfer-associated sets have no native create, local-RD refusal, serial mismatch persistence, and failed create followed by successful retry. These engine tests were syntax checked, **not run**.
- Syntax-only: rendering_device.cpp with TESTS enabled and disabled, plus new engine test TU: exit 0. Commands and actual include roots are in SYNTAX_RESULTS.json. This is not a new host/SDK build or GPU evidence.
- `git diff --check` on owned tracked RD files: exit 0.

The standalone fixture substitutes queue/ref storage and driver, so it proves source decision/return ordering rather than linked-engine, concurrent visibility or fence correctness. Main RD/RT access restrictions remain enforced at the private entry points; no cross-thread observation API is introduced. Existing driver free APIs return void: RETURNED means that exact native driver-free function returned, with no invented native success code or stronger GPU/aggregate release claim.

Only four source/test files belong to this component (hashes in CHANGED_FILES.json). Other renderer/shader changes in the same tree belong to concurrent work and are excluded from this component receipt. Executable scratch files are not deliverables.
