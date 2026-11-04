# Independent native disposal review

Reviewer: `/root/b4_disposal_review` (separate context from implementation). Base: `8355b9a7ae2fb5e0ceaaa8453021865df16add62`. Reviewed the four current source/test files listed and hashed in `RESULTS.json`, the original dependent cleanup and pending-free paths, and the component receipt. Author source hashes matched at review time. No production source was edited.

No blocking source defect found within the stated restricted mechanical-port scope. This is not linked-engine or live GPU acceptance, an E09/source capability, an aggregate `OutputReleased` result, or an owner acknowledgement.

## Source checks

- `rendering_device.cpp:8758`: main RD, original render thread, live driver, valid frame and no active list are checked before factories/free. `_recording_error()` additionally rejects frame draining/advance and retained issue/wait failures. Observation remains possible during recovery, but is restricted to the original RD/RT and cell device identity (`8838`). No public arbitrary-RID enrollment or caller-written witness cell was added.
- Storage factory allocates the cell before its internal native call (`8779`); the internal path reserves backing serial, exact RID and mutable tracker before `driver->buffer_create` (`1649`, `1682`). Failed allocation returns only an empty caller output and releases its unused RID/tracker reservation. No initial upload prefix is possible on this path.
- Uniform factory checks every supplied resource is a live tracked, original-device Core storage buffer, already mutable and without a transfer worker, before the original descriptor path (`8807`). `_buffer_make_mutable` therefore returns false and the storage case's transfer-worker check has no worker to synchronize. Cell and serial exist before the call; exact RID is reserved before the native descriptor allocation (`5433`). Shader is only a layout dependency; no shader physical-return observation is claimed.
- Buffer/set structures and pending-free copies retain the refcounted cell. `free_rid` retains the existing dependent recursion, qualification-busy gates, invalidation order and queues. A successful request means `ENQUEUED` or already `RETURNED`, never a fabricated completion (`8848`). Null witnesses leave ordinary resource enqueue/return behavior unchanged.
- Native-return publication follows, rather than precedes, the exact `driver->uniform_set_free` and `driver->buffer_free` calls (`9473`, `9536`). Device, original RID, original backing/object serial and native ID are matched. Cell transition methods (`native_disposal_witness.h:39`) make identity/transition failures sticky `UNKNOWN`; a later matching return cannot repair that state.
- The maintenance extraction leaves old qualification admission, resource-use checks, free event reservations and owner acknowledgement paths intact. The added empty/invalid-frame rejection precedes modulo/index arithmetic (`8903`). This entry is physical queue progress only; original Core owner obligations must already authorize retirement. It does not turn a frame count or CPU sync into a completion.

## Independently executed checks

`RESULTS.json` records exact commands, outputs/exit codes and four source SHA256 values.

| Check | Actual | Expected |
| --- | --- | --- |
| Original-base return-site fixture | rc1, 2 assertion failures | Target red |
| Current return-site/cell fixture | rc0, 0 failures | Green |
| Publication-before-driver mutation | rc1, 3 assertion failures | Target red |
| Current RD TU, TESTS enabled syntax | rc0 | Green |
| Current RD TU, TESTS disabled syntax | rc0 | Green |
| New engine test TU syntax | rc0 | Green |

The source-body fixture substitutes queue/ref storage and the driver; its results establish decision/identity/return ordering only. It does not establish concurrent GPU visibility, frame-fence correctness, native allocation success, shader creation or real device disposal. The four force-linked engine tests have **not run**. In particular, wrong-thread, descriptor-native failure and maintenance recovery paths were inspected in source but have no new executed linked-engine coverage in this review. No build was started.

Full integration still needs the original Core owner to close operation/lease obligations, retain all native uses, drive maintenance and combine exact physical returns with its own acknowledgement. This review supplies no missing production source association, E09 authority, renderer draw/adoption evidence or overall B4 acceptance.
