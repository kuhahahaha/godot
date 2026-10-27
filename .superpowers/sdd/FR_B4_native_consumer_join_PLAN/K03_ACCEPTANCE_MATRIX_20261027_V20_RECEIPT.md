# K03 acceptance matrix v20 receipt — 2026-10-27

- Preserves matrix v19 and adds `ange_c04_spirv_test_graph_blocker` at ANGE commit `b92836037`.
- C04 evidence: two SPIR-V Catch2 sources compile independently, are absent from SConstruct/default `ange_test`, and the private link probe records missing reflection instrumentation symbols.
- Standalone `ange_gpu_linking_test` remains separately scoped and is not substituted for the omitted tests.
- No source/build-graph change and all authority claims remain `false`.
