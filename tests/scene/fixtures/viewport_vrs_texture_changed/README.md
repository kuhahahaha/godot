# Viewport VRS texture mutation regression

A Viewport must refresh its RenderingServer VRS texture when the assigned
Texture2D resource changes the RID it exposes. Deferred resource notifications
run on the main thread and read the current member, including replacement or
null after an old notification has already been queued.

Build with `tests=yes module_particles_runtime_tests_enabled=yes`. The optional
tests module exposes a read-only observer of the actual render target saved in
TextureStorage; it requires the actual main RD render thread. No production
inspection API or synthetic target state is used.

```sh
ANGE_MACOS_BACKGROUND_RENDER=1 bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/viewport_vrs_texture_changed --script run.gd
```

60 checks cover resource RID mutation, shared viewports, repeated assignment,
replacement, null, detached resources, render-thread-only notification, queued
old notifications and destruction with notification queued. Missing observer or
main RD, a 30-second timeout or any failure gives a nonzero process exit.
`RECEIPT.json` records actual Metal red/green runs: 10 failures before the fix,
zero after. This proves stored target association, not hardware VRS shading or
VRS_UPDATE_ONCE resampling.
