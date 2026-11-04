# Native-disposal fixture cleanup

The fixed2578 host discovers all17 new tests, but NativeDisposal is not clean: its four cases and55 assertions pass while teardown emits dependency_map.size(). Exact one-case isolation confirms only the set-cascade fixture is affected; three other native cases and thirteen independent Guard/Config/Inverse cases are clean. Full raw failed-run/isolated logs and XML remain in the validation package.

The fixture directly erased its fake shader owner, bypassing normal RenderingDevice dependency cleanup. It now releases that shader through free_rid and the existing pending queue. The fake driver handles shader ID31 only for this fixture cleanup; counters ensure the shader stays alive through child return and is freed once, and both dependency maps must be empty. This does not add a production shader witness or claim native shader physical retirement.

Only this test file changes. Its C++ syntax check passes, the independent routine review found no defect, and all seven Metal production hashes remain byte-identical. New-engine execution of the added assertions is pending; old failing records are not upgraded to passes.
