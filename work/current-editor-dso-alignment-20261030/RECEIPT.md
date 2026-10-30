# Current editor/DSO alignment receipt — 2026-10-30

This no-code receipt aligns the latest tests-enabled editor with the explicit ANGE
DSO and reruns the fixed VFX/no-VFX/invalid-VFX probes. Editor SHA and DSO SHA are
recorded in `EDITOR_SHA256.txt` and `DSO_SHA256.txt`; the 15-selector recheck is
copied by reference in `SELECTOR_RECHECK.json`.

All 15 selectors are rc0: K03/P2 25/386, QualificationNative Metal 2/51,
QualificationNative Vulkan 65/251, MetalCompletion 6/386, ParticlesReadback 2/5,
E05 18/417, QualificationRt 6/458, TypedABI 12/356, SceneReady 2/52,
Qualification 102/1896, device_exit 133/2619, QualificationNative 67/302,
K03 44/1153, R02 9/115, RenderingDevice 12/214.

The three current pair probes each return rc2 at
`host.storage_owner_transport/E05/UNAVAILABLE`, before world admission. This is
expected fail-closed negative evidence. No owner, submit, completion, readback,
Ready, GPU, or test-authority claim is made.
