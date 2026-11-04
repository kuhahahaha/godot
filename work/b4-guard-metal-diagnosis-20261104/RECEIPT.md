# Metal guard diagnosis receipt

Outcome: **confirmed Metal backend auxiliary-size-table ABI defect; not fixed**. Detailed source chain, evidence, minimal implementation slice and targeted tests are in `DIAGNOSIS.md`.

- Independently inspected original and record-gating candidate SPIR-V for all five guard modes: all validate; every mode still contains OpArrayLength.
- GPU author exported six Metal containers through the existing frozen main-RD host with root authorization, without pipeline creation/dispatch. This reviewer independently extracted all six actual MSL stages and verified each against its serialized StageData SHA256 (`MSL_EXTRACTION.json`).
- All five original variants collide synthesized spvBufferSizeConstants at argument ID0 with the first real storage buffer. INIT original's invalid cast is exactly line636 from the real failing pipeline log. The candidate merely changes that cast to active OutputMetadata; VALIDATE also collides with active CurrentRecords. Unused descriptors are therefore not the cause.
- Source review establishes both missing auxiliary remap and missing runtime byte-length table allocation/population/binding/retention. Reserving an index alone cannot fix the runtime ABI. The guard's valid length checks should remain.
- Public uniform supersets are matched against the reflected set and only reflected entries reach the driver. Vulkan's OpArrayLength diagnostic is optional re-spirv parser fallback, not Vulkan GPU rejection; its separate main-RD evidence remains scoped to Vulkan.
- Standalone Metal syntax check was unavailable because the Metal Toolchain component is absent (`MSL_DIAGNOSTIC_CHECKS.json`). This environment failure is not counted as a shader red. No engine build, production source modification, backend repair or successful Metal guard GPU execution occurred.

## Follow-on implementation boundary

This can be a separately owned Metal backend task, with B4 guard GPU acceptance as its consumer. Concrete files: `drivers/metal/rendering_shader_container_metal.{h,cpp}`, `drivers/metal/rendering_device_driver_metal.{h,cpp}`, `drivers/metal/metal_objects_shared.h`, and `drivers/metal/metal3_objects.cpp`; exact serialized metadata/cache version and binding-visible length representation must be decided together. No thirdparty algorithm change is needed merely to repair the conflicting mappings.

Before a new engine build, work can retain the real failing-host RED and hash-verified MSL oracle, add source-level metadata/index allocation and range-model tests, use the frozen host's translation-only export for unchanged-input controls, and test serialization/reference length-table models. If a lightweight standalone harness is authorized later, it may invoke repository SPIRV-Cross with the proposed remaps to test emitted binding IDs; that is translator-level evidence only. Source-only changes cannot demonstrate the frozen host's driver binding or GPU GREEN. Full repair acceptance requires a rebuilt affected backend and actual Metal pipeline/dispatch, lengths, failure unwind and resource-retirement checks.

`MANIFEST.json` hashes this complete diagnosis package; `REVIEWED_FILES.json` hashes inspected production sources. Actual export command/host hash is in the sibling `b4-guard-main-rd-gpu-20261104/metal-container-export.json` (a copy is retained here for standalone provenance). No artifact in this package claims the backend fix passed.
