# Instance192 engine guard lifecycle review

Reviewer `/root/review_vertex`; read-only review of D_INSTANCE192_CREATE_IMPLEMENTATION_MAP.md section1, actual ParticlesStorage/renderer shutdown, guard wrapper and ShaderRD cleanup. No production/test edits, builds, tests or Git operations. Selection files are owned by Astra/root and were not changed here. This document specifies a minimal integration boundary, not an implemented or GPU-validated feature.

## Conclusion

The actual normal renderer lifecycle can own the fixed five-variant ParticlesExternalGuard bundle in ParticlesStorage. Normal construction and normal destruction run on the main renderer's assigned render thread while the main RD exists. The proof that original production external owners have finished must come from RenderingServerDefault::finish's existing fail-closed service gate BEFORE renderer destruction. ParticlesStorage's destructor does not itself retire external owners, and the later RD finalizer must not be described as performing that owner retirement.

The main current gap is straightforward source ownership: ParticlesStorage has no guard field, preparation/status or explicit release. Its fixed five shaders/pipelines currently exist only in independently instantiated guard wrappers, including a native test fixture; they are not an engine-owned bundle available to Create.

## Actual initialization chain and identity

RenderingServerDefault::init (rendering_server_default.cpp:348-362) assigns the dedicated rendering worker and queues _init there when create_thread is true; otherwise it sets server_thread to MAIN_ID and calls _init inline. _init (305-324) constructs the RD compositor through RendererCompositor::create. RendererCompositorRD's constructor (315-368) creates MaterialStorage, MeshStorage and then ParticlesStorage, before canvas/scene renderer construction. The normal ParticlesStorage constructor (particles_storage.cpp:56-64) already initializes renderer effects such as SortEffects; this is a suitable engine initialization point for the fixed bundle.

Guard _require_main_render_thread requires live RenderingServer, rs->is_on_render_thread, nonnull RD singleton and equality to rs->get_rendering_device. The compositor pointer is not yet assigned back to RSG::rasterizer during its constructor, but this causes no false refusal: RenderingServer::get_rendering_device (rendering_server.cpp:1884) directly returns RenderingDevice::get_singleton and does not dereference RSG::rasterizer. RenderingServerDefault::is_on_render_thread uses the already assigned server_thread. Keep the wrapper's original check; do not bypass it for startup or a local RD.

The private ExternalLinkedOwnerTest constructor (particles_storage.cpp:49-54) deliberately sets only singleton and its CPU component flag. It does not prepare effects. Its early destructor path must remain free of normal shader/material assumptions. An added guard pointer defaults null and status unconfigured in this path, or an unprepared value member remains empty. Tests must not turn that constructor into ready=true or synthesize valid RIDs.

## External-owner retirement precedes engine guard release

Normal shutdown is RenderingServerDefault::finish → _finish → RendererCompositorRD::finalize:

1. finish first calls require_close_success(Registry::get_singleton().service_stopping(1<<5),LAST_REF) (rendering_server_default.cpp:364-365). Registry::service_stopping (gdextension_guard_enrollment.cpp:150-172) returns ERR_BUSY while ANY registered production attachment still has held_device, including quiesced/revoked owners; it also checks unfinished service-dependent qualified levels. require_close_success terminates on failure, so _finish and storage destruction do not follow a failed gate. This is a refusal boundary, not an automatic drain scheduler.
2. Original production attachments retain the RD entity edge until requests and children close. production_close checks metadata_active, every request OWNER_CONSUMED, and further request obligations before returning the attachment; production_entity_return_locked refuses children and only then clears held_device (gdextension_production_request.cpp:2463-2511). Thus a legal installed generation/slot or pending metadata is not allowed to survive by merely dropping a public particles RID.
3. _finish (rendering_server_default.cpp:327-345) finalizes canvas and rasterizer while RD still exists. RendererCompositorRD::finalize (173-185) deletes renderer scene/canvas/fog, then ParticlesStorage, then light/mesh/material/texture storage. Main RD finalization on the dedicated render thread occurs AFTER those deletions. Single-thread mode retains the same _finish ordering, with RD destruction later on its owning main thread.
4. External owner retirement itself remains bounded original OBSERVE service: ParticlesStorage::_production_output_service generation branch runs native retirement service, checks generation_returnable, returns directories as a separately charged phase, calls original generation_return, then marks RELEASED (particles_storage_retire.cpp:141-199). generation_returnable rejects every live slot/storage/surface/preparation pin/group/custody and open history/view (external_output_owner.cpp:100-126).
5. particles_free (particles_storage.cpp:253-282) withdraws and preserves a non-RELEASED external owner. Only RELEASED permits original CPU destruction and metadata_return. It does not provide a shortcut for guard release. Future unpublished Create failures must follow the same retained owner service and terminal cleanup path described by the implementation map.

The engine guard can therefore outlive every AUTHENTICATED production use when this service/attachment lifecycle is preserved. Do not add direct memdelete of external owners, a destructor drain loop, a private scheduler or a replacement root merely to make teardown appear safe. Direct test-only owners/local Registry fixtures do not inherit the global service-stop proof; their fixtures must explicitly retire/destroy originals before deleting test storage. Current public render-profile Create gates prevent an unrooted Instance192 engine user; keep them until root/selection/failure-drain integration is complete.

## Guard prefix failure and true release semantics

Guard prepare_pipelines checks original owner/thread and limits before creating anything. It initializes five variants INIT, VALIDATE, FINALIZE, INIT_EMPTY and FINALIZE_EMPTY, creates one ShaderRD version, obtains real shaders, checks version validity and creates real compute pipelines. ready is true only after all five succeed. Repeat preparation while ready preserves the same RIDs.

On a failed shader/version readiness or pipeline ordinal, _clear_pipeline_prefix marks ready=false, frees any existing pipeline RIDs in reverse mode order, then calls shader->version_free. Important counting detail: version_get_shader may initialize/compile every enabled variant group, so a pipeline failure at ordinal i does NOT mean only i+1 shader objects exist. Correct cleanup must free all native variants actually owned by the ShaderRD version, not an invented shader prefix count.

ShaderRD::version_free (shader_rd.cpp:933-960) first owns/locks the original version and calls _clear_version; _clear_version (263-278) waits for compilation tasks and frees every valid variant RID before clearing native variant storage. This CPU compile join is not a GPU-completion proof. The guard then clears shader_version only after successful version_free. Release deletes the now-empty shader wrapper, sets shader null and initialized=false only on successful cleanup. Version creation failure leaves no version/pipelines but may retain the initialized empty shader wrapper until retry/release; that is valid local cleanup state, not a prepared bundle. A version_free ERR_BUG must not be hidden by zeroing its handle or claiming readiness.

RD compute_pipeline_is_valid means the original compute_pipeline_owner owns the RID (rendering_device.cpp:6217), and actual compute_pipeline creation fails before owner insertion if driver creation fails. Thus the prefix cleanup's validity check does not strand a successfully created owned pipeline merely because the driver reported create failure. Standard RD free_rid requests logical/deferred disposal; actual driver pipeline/shader frees occur in RD pending-frame cleanup/finalization after relevant GPU work. Neither release_pipelines nor the existing test's owner-entry removal proves physical return.

Guard destructor refuses wrong-thread/main-owner cleanup rather than freeing native objects on an unrelated thread. Keep that fail-closed behavior. Explicit engine release on the valid RT makes subsequent guard destruction empty. There is no need to create a producer-owned disposal group for the fixed engine bundle: generation-owned descriptors and command uses still require their existing original physical retirement, while the bundle itself follows the renderer's ordinary resource lifecycle.

## Minimal source integration for root

- particles_storage.h: add a private ParticlesExternalGuard pointer (with forward declaration) and real Error status, initialized null/ERR_UNCONFIGURED; alternatively an empty value member if including the wrapper header is acceptable. Keep access internal; no public ABI or producer-writable ready flag. Existing sort_effects vicinity is appropriate.
- particles_storage.cpp normal Effects initialization: create the wrapper once and save prepare_pipelines' actual result. Engine startup should remain able to run other profiles if the optional bundle is unavailable. Future Instance192 Create must require saved OK AND actual is_prepared; return its legitimate unavailable/preparation status before any producer native effects. No lazy factory under the producer request should be called engine-owned.
- same normal destructor, before singleton/RD/storage disappearance: explicitly release on the original main RT, verify success instead of overriding its ownership checks, and delete only an empty/successfully released wrapper. The private component-test constructor must leave it empty; do not introduce an unconditional dereference in its early destructor path. Existing RenderingServer finish gate supplies owner ordering; no renderer finalize reordering is necessary on the inspected production route.
- future Create/Seal code should consume only actual internal guard getters after checking status/readiness and ownership. Preserve exact shader/pipeline association per mode. Root coordinates these header/storage changes; this reviewer has not changed them.

## Required directed tests and limits

Existing test_external_output_guard.cpp exercises pure layout preflight only. Existing ParticlesExternalGuardTestFixture::run (module test fixture lines80-181) constructs a LOCAL wrapper on actual main RT, observes five distinct original shaders/pipelines, repeat identity, release, reprepare and release; it explicitly claims no dispatch or physical return. It does not establish ParticlesStorage engine ownership, preparation failure cleanup, or external generation-before-engine teardown ordering.

For the engine lifecycle change, extend original fixtures rather than fabricate readiness:

- Normal real storage/renderer startup exposes a prepared bundle with all five original owner IDs/layouts, repeated accesses preserve IDs, and Create cannot consume an unavailable bundle. CPU component constructor reports unavailable and allocates no guard effects.
- Actual shader/version failure plus each actual pipeline-create ordinal0..4: retain driver create/free observations and verify successful pipeline prefix is released exactly once, all actual version-owned shaders are removed, ready/getters remain empty and retry/release never duplicate ownership. Failure injection belongs at the original driver factory boundary, not at a wrapper setter that substitutes success/fake RIDs.
- Wrong thread/local RD refusal must leave ownership untouched; successful original RT release then destroys only empty state. Do not intentionally let a wrong-thread owning destructor silently leak as a passing cleanup test.
- While a production generation/slot/metadata attachment remains, the renderer-service stop gate refuses before engine bundle deletion. After original bounded owner retirement, RELEASED particles_free and attachment close, valid RT teardown releases guard before RD finalization. A refusal observation must not actually tear down the renderer or fabricate completion.
- Real native fixture can inspect original storage bundle (not just a local temporary), then exercise engine shutdown with diagnostic inspection. Standard mock/CPU tests establish bookkeeping; real shader/pipeline preparation requires the native GPU fixture and pinned backend host. Even native preparation without dispatch does not prove Instance192 validation/render output.

No build/test result is claimed by this read-only task. Rendering lifecycle support is source-confirmed subject to preserving the actual service-stop/root/metadata gates; full Instance192 Create remains gated on the separate transaction and retirement work.

Read time: 2026-11-07T02:01:01.410388+00:00

- `servers/rendering/renderer_rd/storage_rd/particles_storage.cpp`: `f3001bde8f53b4e147e94dd7a08442fbc4a6ffabe7c3d34511435ec7c5c12480`
- `servers/rendering/renderer_rd/storage_rd/particles_storage.h`: `8c3bd10ed2bde5ddc76d81b7bed91653f4da6738c854a4c2ee0aa5336f6b3aa2`
- `servers/rendering/renderer_rd/effects/particles_external_guard.cpp`: `232859ad217e472cba00b7ad7f211795b190b578a5e430ebad7e17acb747785b`
- `servers/rendering/renderer_rd/renderer_compositor_rd.cpp`: `def96d4762332133e099fa78045edfdf9fa8e0135dee30d18727bf65eaef7876`
- `servers/rendering/rendering_server_default.cpp`: `892a2a4153c3ec501cec5672b71ebbd766fc7199e1794a9692bf7c29018a9949`
- `core/extension/gdextension_guard_enrollment.cpp`: `0aeb7c5d8a79f741f13bef0c1b9b8e7867c951e4275f91c073d41a83e67ae624`
- `servers/rendering/renderer_rd/shader_rd.cpp`: `00a5b7ecb9640a520f2a05263a0d2805afa5bd4c0a92267288260c4ea24f5a82`
