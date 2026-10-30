# Independent review — shader/pipeline retirement audit

Read-only review of shader/pipeline owner state, qualification retire ordering, frame disposal, Metal readiness, and focused tests.

1. Pipeline Unknown/identity mismatch blocks shader enqueue; the reducer keeps the report Unknown/error-shaped and does not publish Returned.
2. `_free_internal()` and `_begin_frame()` independently refuse a qualification shader until the driver reports retirement-ready. Render shaders require both Metal libraries; compute shaders require the kernel.
3. Frame disposal frees pipelines before shaders, and qualification free events are published only after physical disposal.
4. Existing controls pass for unknown pipeline, identity failure, possible dependency, and non-error Unknown with a dependent blocker.
5. Current runtime reaches E05 owner-unavailable before shader/pipeline admission, so it cannot prove positive native retirement; this is reported as a boundary, not hidden.

No source modification is warranted in this audit package. The next positive coverage would require the authenticated upstream owner transport and a real qualified pipeline pair.
