# System scalar product pair intake — 2026-11-04

This intake checks the present Godot native side for the first-stage ANGE System/Method request and leaves the six-link gate open. It is not a product runtime receipt.

## Fixed Godot side

- `bin/godot.macos.editor.arm64` SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; adjacent controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`.
- Selected core/compiler stamps are `582349c655b6f9656a739f8fbf11eab5feff13d8fb1a9d8dbb08c185daef6139` / `e802f3482d53be251cada1f8971d18a77867d1d14b48ac6335a51f663a8e2e09` in `core-selection/CoreBuildSelection.schema1.json`.
- `core/extension/gdextension_interface.json` SHA-256 `26e0ab13125437131989891f2cee8ba3a8f5e9d2d76effb8f2bdbdd523db17b4` is byte-identical to `/Users/macbookair/Documents/Work/ANGE_phase1_integration/thirdparty/godot_api/gdextension_interface.json` at this intake. It declares the six System scalar V1 functions; `core/extension/gdextension_qualification.cpp` registers them.
- The current fixed editor reran `--headless --test --source-file='*test_record_source.cpp,*test_qualification_rt.cpp' --no-colors`: rc 0, 15/15 cases and 717/717 assertions. Raw output is `FOCUSED_RECHECK_20261104.log`. This is a native control, not a GPU or original issuer run.

## Pair and caller boundary

The existing `/Users/macbookair/Documents/Work/ANGE_phase1_integration/bin/libange.macos.template_debug.arm64.native_test.dylib` identity sidecar pins core/compiler stamps `1da8f74011c7992723d1cd5c47aaacc35f480898ad8a2a3bb1cdd97e1ea9548b` / `39a3123a771d415829a5f3f909f8fcb3f748a3caf0a958637aaaec2c8093d166`. It belongs to the no-trace seal-wait candidate editor, not the fixed editor above. That DSO must not be paired with the fixed editor for product acceptance. The ordinary product DSO also predates current phase1 System E09 edits; its existing artifact does not prove those edits are compiled.

The phase1 System native-test caller in `tests/native/native_particle_system_cases.cpp` compiles a fixed scalar source, starts a checked System request, expects binary32 output word `0x40400000`, and checks close authority. The current main ANGE tree still lacks the coherent Host API/binding/System issuer source package, while phase1 owner edits are active. The fixture's prior Metal World/System/Cancel pass on the separate no-trace matched pair is retained in `work/particle-render-exit-seal-audit-20261105/SEAL_WAIT_CANDIDATE_RECEIPT.md`; it does not provide per-ticket product identity and exact raw result capture. No new DSO was built or run for this intake.

Next executable gate: receive the coherent original System/Method issuer and a DSO built against one selected Godot core/SDK identity, then capture original request identity, same ticket's issue/completion facts, raw exact readback, physical set/resource return, release and close/drain. Use `work/particle-native-six-link-audit-20261102/RUNBOOK.md` and its acceptance table for success/failure/cancel source-aware review. Only after that gate proceed to Sprite visible production consumption on the same accepted owner path.
