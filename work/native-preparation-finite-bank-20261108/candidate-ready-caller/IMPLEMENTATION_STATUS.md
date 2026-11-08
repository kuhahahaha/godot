# Backend ready caller prerequisite

The clustered and mobile external preparation callers now perform an exact, side-effect-free ShaderRD variant lookup before `is_valid`, variant getter, input-mask cache, geometry derive, pipeline creation, or material hold. Variant formulas remain backend-owned; Mobile retains fp16 indexing. Refusal leaves row state and outputs unchanged.

Build exit 0. Original Metal main-RD guard fixture: Mobile 202 checks/process0 on Apple M3 Pro Metal4.0; Forward+ 202 checks/process0 on same host. These checks exercise the existing actual shader/pipeline/material owner fixture but do not dispatch GPU output or prove physical retirement.

Factory output paths now reuse the already-ready RID through `get_native_shader_if_ready`; ordinary renderer pipeline cache paths retain the original effectful getter. This still is not the full prospective G gate.
