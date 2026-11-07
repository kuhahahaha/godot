# Independent guard candidate review

Reviewed `work/instance192-engine-guard-candidate/01-engine-guard-lifecycle.patch` and `02-component-guard-test.patch` against original production sources. Read-only review; no build, test, git or active source change. No confirmed production defect found in this lifecycle wiring. Acceptance remains source-reviewed, not normal-lifecycle runtime verified.

## Initialization and error state

Normal ParticlesStorage constructor's Effects section is reached from RendererCompositorRD construction after MaterialStorage and MeshStorage exist. RenderingServerDefault::_init constructs the compositor on the assigned render thread. The guard's `_require_main_render_thread` reads RenderingServer and RD identity; RenderingServer::get_rendering_device simply returns RD::get_singleton, so there is no dependency on RSG::rasterizer assignment completing first. Preparing the real five variants here is not a fabricated readiness flag. Saving the exact Error while retaining the wrapper is coherent for optional profile unavailability. Create must still check actual ready state and this result before exposure; this patch correctly does not remove the profile gates.

## Teardown and service-stop gate

RenderingServerDefault::finish calls require_close_success(Registry::service_stopping(1u<<5)) before queuing/calling `_finish`. Registry refuses any production attachment with held_device, including quiesced owners. `_finish` invokes compositor finalize before threaded RD finalization. Compositor deletes ParticlesStorage before MeshStorage/MaterialStorage and before later RD destruction. Thus normal shutdown gives guard release its original main-RD render-thread context after original external attachments have returned. The comment is valid for this normal route, not a guarantee for arbitrary direct `memdelete(ParticlesStorage)` callers.

The new release CRASH_COND is deliberately terminal on wrong-thread/release-error; it does not silently destroy a wrapper whose native objects could not be returned. No graceful recovery is claimed. Root should preserve the existing fail-closed service-stop tests; the added component test does not exercise this gate.

## Double release and failed prefix ownership

`release_pipelines` calls `_clear_pipeline_prefix`, then deletes shader and sets shader=null and initialized=false only on successful cleanup. The immediately following wrapper destructor sees shader=null and does nothing: no second version_free or pipeline free occurs. Successful explicit release is safe before memdelete.

Limits/main-thread failure before shader creation retains an empty wrapper and exact error. Shader-version creation failure retains an initialized shader wrapper; final release safely clears the empty prefix and deletes it. Actual shader/pipeline creation failure invokes reverse pipeline cleanup and ShaderRD::version_free. `_clear_version` waits for compilation and requests frees for every actual variant, not merely the number of completed compute pipelines. If version_free fails, shader_version and shader wrapper remain held; teardown aborts on the repeated error rather than claiming return.

Pipeline/shader free requests are logical owner retirement. RD can retain actual native objects in deferred disposal after the wrapper has cleared its RIDs. This is existing engine RD ownership, not a generation disposal witness or immediate physical-return proof. The patch introduces no borrowed producer pin or group and cannot be described as successful generation-native allocation accounting. No change here proves that each failure prefix physically returns; directed actual driver failure and shutdown observations remain necessary.

## Test boundary and remaining evidence

The new test exercises only ExternalLinkedOwnerTest's deliberate minimal constructor and early destructor. It proves null guard, ERR_UNCONFIGURED and false readiness survive that component path, and singleton is cleared. It cannot detect a missing normal ctor prepare, a missing normal destructor release, wrong native variant association, failure-prefix leak, or service-stop bypass. The candidate README accurately states this limitation.

Before marking full guard lifecycle verified: actual normal storage member observations for all five variants and stable shader/pipeline association; each real shader/pipeline failure prefix; original service-stop refusal with live generation plus actual drain; native disposal counts after normal teardown. Existing local wrapper fixture is useful adjacent evidence but is not the new engine-member lifecycle test. No GPU or Instance192 acceptance claim follows from this review.
