# Independent review — Metal shader retirement readiness

Review basis: read-only source inspection of `RenderingDeviceDriverMetal::shader_retire_ready_nonblocking()`, `MDComputeShader`, `MDRenderShader`, `MDLibrary`, and the RenderingDevice qualification gates; rebuilt editor and reran focused retirement tests.

## Findings

1. **Fixed P1/P2 local issue:** the old implementation dynamically cast only to `MDComputeShader`. A valid render shader therefore returned `false` regardless of vertex/fragment compilation completion and could not pass the nonblocking retirement gate. The patch now checks both render libraries and returns false when either is absent or not ready.
2. **Preserved fail-closed behavior:** an unknown shader object, null library, or in-flight library remains not-ready. No event or owner state is fabricated.
3. **Pipeline-Unknown reducer behavior remains intentional:** the existing `unknown pipeline preserves its original shader` test passes (1/1, 30 assertions), and the identity/error precedence tests pass. This package does not weaken that contract.
4. **Runtime scope remains bounded:** the fixed editor/DSO pair reaches Metal startup but stops at ANGE `E05 host.storage_owner_transport` with rc 2. This is the first external owner boundary; it is not evidence of a Godot/Metal crash or GPU readiness.

## Limitations

The current U01 fixture cannot exercise a qualification-owned render shader because E05 stops scene installation before resource ownership. A future owner-complete fixture should verify that a render shader with one in-flight library remains queued, then retires once both libraries report ready. Existing compute and reducer tests remain green.
