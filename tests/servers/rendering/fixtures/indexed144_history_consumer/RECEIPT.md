# Indexed144 history shader consumption

Forward+ and Mobile now read the packet's dedicated vertex-history-validity buffer only for Indexed144 motion. Validity requires an in-range vertex and canonical word1. Invalid history selects current attributes and writes literal zero motion. Float144 bytes stay unchanged. The packet provider must still retain and bind the exact readonly validity/VA/material/source resources.

The first candidate collided with the production ShaderCompiler user-varying start at location15. Independent review found this, and a real ShaderCompiler material produced the counterexample. The fixed version uses the existing reserved location10; Forward+ packs the original instance index and history separately, Mobile keeps profiles mutually exclusive. Ordinary and Instance192 interfaces retain their original branches.

Actual-tree verification passed 42 full-template SPIR-V compile/link commands. The real generated user varying made the old candidate fail all six stage/link commands on location15 overlap; the fixed candidate passed all six. The generic codegen fixture's ALBEDO identifier was adapted to the native variable name only in the harness, as recorded in DELIVERY.json; this is not end-to-end production material-action proof.

The initial engine build and full CPU suite passed:2070 cases/517049 assertions. B4 neighbors passed25/647, retaining420 stderr bytes from three existing camera-token refusal negatives; the guard group passed6/521 with empty logs. Full-suite stdout571/stderr4017 retain existing negative diagnostics. The actual repository GLSL formatter was then applied: all42 SPIR-V files remained byte-identical and include ordering stayed unchanged. The final rebuild passed with frozen inputs; pipeline-key1/2 and guard6/521 checks passed with empty logs.

This delivers shader-source, SPIR-V and CPU-neighbour evidence. It does not certify live GPU motion, native packet adoption, source/window authorization, budget admission or physical retirement. Those remain active work in the complete native-output package.
