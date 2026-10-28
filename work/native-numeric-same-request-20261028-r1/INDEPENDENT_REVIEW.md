# Independent native substrate control review

Reviewer: /root/native_numeric_control_review; read-only review of full source, original factory/contracts, immutable generation inputs, final results and logs. No edits/builds/tests by reviewer. Initial actionable P2: checked SPIR-V and copied reflection grants were not bound to native containers consumed by creation. Author added observed generation before/after pins, exact original SPIR-V matching, source/SPIRV/container/Core/compiler identity receipt, compiled expected hashes, runtime pre-effect checks, and use of the same retained validated native container vectors.

Final response (verbatim):

No remaining actionable P1/P2 findings in the reviewed native substrate control. The previous P2 is resolved: generation pins the source/SPIR-V/container chain, the probe checks those hashes before native effects, and native creation uses the same retained container vectors.

I inspected the final source, generation receipt, RESULTS.json, and raw logs. Both selected cases recorded rc0 and passing checks. Generation and run before/after pins match; current pinned files also match. Positive execution records 13 completed native facts and an exact 104-byte owned readback. Cancellation records the actual upload prefix’s three completed facts, refuses further recording, and produces no readback. Both show physical resource returns, bridge closure, SDK termination, CodeHold release, and the DSO finalizer before manager unload returns.

Scope remains one fixed Metal occurrence on the selected `--render-thread safe` path. This does not establish separate-render-thread behavior, full corpus coverage, a real ANGE production request, two-stage Verified qualification, U08/Ready/G1, or OS image unmapping.

Read-only re-review; no builds or tests executed.
