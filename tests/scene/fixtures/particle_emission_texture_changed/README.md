# Particle emission texture changed regression

Run with an actual main RenderingDevice. The fixture uses public `RenderingServer.material_get_param()` on the render thread to inspect all three emission texture parameters and the point-validity flag. Two differently colored ImageTextures remain alive throughout. An AtlasTexture changes its underlying RID, shares three slots across two materials, and exercises same-reference setters, null RID/recovery, render-thread signal emission, per-slot replacement/null detach, queued old notifications, receiver destruction, and remaining shared-slot propagation.

```sh
bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/particle_emission_texture_changed --script run.gd
```

Require exit 0, exactly one `PARTICLE_EMISSION_TEXTURE_RESULT` with no failures, and no engine/script errors. The test uses no new native observer and no local/dummy RD. It verifies actual material parameter association; it does not claim GPU sampling, particle motion, or physical effects. Parameter subscriptions are independently checked per slot. Point validity follows the existing non-null resource-reference semantics even when the AtlasTexture currently has no underlying RID.
