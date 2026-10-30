# PipelineDeferredRD / shader-pipeline retirement audit

`PipelineDeferredRD` creates pipelines on a WorkerThreadPool task and enforces an
explicit `_wait()` before `free()`. `free()` waits the task, validates the resulting
pipeline RID in DEV builds, frees it through `RenderingDevice`, and clears the RID.
The destructor rejects a live pipeline, preventing a dependent shader from being
freed first. `_start()` calls `free()` before scheduling a replacement task.

The source census found explicit pipeline cleanup in the owning effects/storage
lifetimes (CopyEffects, ParticlesStorage, Fog, BokehDOF, GI, SSEffects, FSR and
related shader data). No owner destructor or replacement path leaves a deferred
pipeline task live while its shader is released. RenderingDevice additionally
checks `shader_retire_ready_nonblocking()` before deferred shader disposal and
returns `ERR_BUSY` while any shader retirement dependency is unknown.

Current harness coverage reaches the production reducers and Metal completion path:
Qualification unknown-pipeline and shader-retirement cases, QualificationNative
Metal, MetalCompletion, and the focused shader identity cases all pass. There is no
reproducible RED in this tree and no safe production change to make. The remaining
runtime pair still fails earlier at E05 owner transport, so it cannot exercise this
pipeline path end to end.
