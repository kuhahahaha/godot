# Backend ready caller prerequisite

The clustered and mobile external preparation callers now perform an exact, side-effect-free ShaderRD variant lookup before `is_valid`, variant getter, input-mask cache, geometry derive, pipeline creation, or material hold. Variant formulas remain backend-owned; Mobile retains fp16 indexing. Refusal leaves row state and outputs unchanged.

Build exit 0. Original Metal main-RD guard fixture: Mobile 202 checks/process0 on Apple M3 Pro Metal4.0; Forward+ 202 checks/process0 on same host. These checks exercise the existing actual shader/pipeline/material owner fixture but do not dispatch GPU output or prove physical retirement.

Known boundary: `create_native_pipeline` still re-queries the effectful getter, so this is a prerequisite and not the full prospective G gate.
