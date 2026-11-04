# System scalar native source integration — 2026-11-04

This milestone freezes the existing shared-tree System scalar implementation and its necessary V3/original-operation checked-append prerequisites into a fetchable Git source revision. It is a source/ABI reproducibility milestone, not a new matching host qualification or complete production-output acceptance.

The earlier description of f7bc2b053d as a fetchable “System-capable” source version was inaccurate: that revision contains the Vulkan qualification change, while System scalar implementation/JSON changes were still dirty. This package corrects that source-delivery gap. The original running System host/SDK pair is unchanged.

## Scope and provenance

Base: f7bc2b053d80c9d04aa1b44192e27044927f9950. SOURCE_MANIFEST.json lists the 15 frozen files and their SHA256/size. Each file was copied byte-for-byte from the shared development tree; an independent Astra agent rechecked those bytes and the dependency closure. Shared tracked files, index, branch, foreign changes, and binaries were not edited by this packaging operation. B4 work lives in a separate worktree and is excluded.

Includes six device_session_system_scalar_*_v1 interfaces, their JSON structures, registrations, implementation, bridge/code holds, completion/readback/native frame integration, internal set physical-return handling and release checks. V3 qualification record/original-operation identity and checked signatures are shared prerequisites included explicitly, not additional System APIs. System record currently requires direct main-RD render-thread invocation; no System RT kind is added.

Existing core/extension/SCsub compiles *.cpp and generates the interface header from JSON. Existing gdextension_device_session.cpp calls register_qualification_interfaces. Existing tests/SCsub recursively includes/force-links test sources. No build wiring, reload code, main-loop probes, or other particle dirty changes are needed.

## Verification boundaries

The official interface generator accepted the frozen JSON and generated gdextension_interface.gen.h SHA256 ec25549317a591c3009de34f217bcb835f1d3cc52ba2d8782bde2af15d5dbad7. All 10 frozen C++ files passed syntax checks. Syntax checks use an overlay of frozen files above the isolated Godot base headers; exact commands/results and raw warning logs are retained. RenderingDevice needs its unchanged baseline local compat include copied into that overlay to avoid including a second physical RenderingDevice header. Initial overlay include failures are retained.

No new engine build, link, doctest run, live GPU run, or matching host/SDK pair was performed for this composition. Existing 112/102 and scalar/cancel/close host receipts may describe their original bytes only; they do not qualify this new source revision. The added tests cover checked identity and V3 admission, not System GPU or retirement fault behavior.

## Open production correctness work

Independent review found three existing issues in the frozen implementation; this package does not silently fix or qualify them:

1. Unknown checked append (RenderingDevice returns no valid handle with Unknown) is only tested via body handle in system_scalar_record. No mapping_failed flag is set; publish can classify empty source rows as no_effect and return pins. Exact chain: rendering_device.cpp checked dispatch → gdextension_system_scalar.cpp dispatch/append_error → gdextension_qualification_ticket.cpp no_effect/failed-prefix retirement. No native Unknown injection was run.
2. System record does not perform native/fact capacity admission before dispatch/readback. Reserving FACT_CAPACITY storage does not prove the driver bound. Overflow is detected only later during native issue, after a prefix may exist.
3. system_set_return_error is written but not published/read; maintenance errors after ticket physically_retired may be suppressed. state8 computation/readback completion is not internal-set physical release; release can remain Busy without the retirement error being surfaced.

These remain explicit queue items for separate reviewed fixes and fault tests. They are not a reason to call the whole ongoing goal complete. No B4 record token, RecordedOrdered, renderer adoption, production lifecycle qualification, or physical-release success is claimed here.
