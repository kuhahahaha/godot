# Current tests-enabled editor build and native rerun — 2026-10-30

The current dirty Godot source built a tests-enabled editor with rc0. Binary SHA: `fb8693bd5c159a724793b17f23581b1fdc2e1ba9875c0195834883ec79cc736a`.

- `K03_P2` `[K03][P2]*` rc 0
- `QUAL_METAL` `[QualificationNative][Metal]*` rc 0
- `QUAL_VULKAN` `[QualificationNative][Vulkan]*` rc 0

This is native substrate evidence only; no production particle issuer, same-request readback, Ready, or G1 authority is claimed.

Boundary selectors on the same tests-enabled editor:
- `METAL_COMPLETION` `[K03][MetalCompletion]*` rc 0
- `PARTICLES_READBACK` `[ParticlesReadback]*` rc 0
- `E05` `[Qualification][E05]*` rc 0
- `QUAL_RT` `[QualificationRt]*` rc 0
- `TYPED_ABI` `[DeviceExit][TypedABI]*` rc 0

The superseded no-tests build diagnostic is retained in `DIAGNOSTIC_NO_TESTS.md`; it was a test-support configuration failure, not a native selector regression.

Issuer/recorder selector diagnostics on the same editor: `[Qualification][E06]*` and `[Qualification][E09]*` both matched 0 cases (rc0 with no assertions). They are not coverage evidence and no issuer/recorder conclusion is drawn.

SceneReady selector `[DeviceExit][SceneReady]*` rc 0; output is recorded in `SCENE_READY.log`.

Full qualification selector `[Qualification]*` rc 0; output is recorded in `QUALIFICATION_FULL.log`.

Full device-exit selector `*device_exit*` rc 0; output is recorded in `DEVICE_EXIT_FULL.log`.

Full native qualification selector `*[QualificationNative]*` rc 0; output is recorded in `QUAL_NATIVE_FULL.log`.
