# Particle collision texture changed regression

Run the original main RenderingDevice with `tests=yes module_particles_runtime_tests_enabled=yes` and 3D enabled. The test creates two native 3D textures on the render thread and observes the actual RD particle collision owner's field RID and resolved 3D texture through a read-only test observer. It covers both SDF collision and vector-field attraction, shared resources, two clear/rebind cycles, render-thread changed emission, repeated same-reference setters, replacement, null, deferred old notifications, receiver destruction, and caller-owned source lifetime.

Example on macOS:

```sh
bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/particle_collision_texture_changed --script run.gd
```

Require exit 0, one `PARTICLE_TEXTURE_RESULT` with no failures, and no engine/script errors. Exit 77 means the required test module/observer is unavailable. This observes actual native resource association; it does not claim particle motion or GPU shader sampling coverage. No local RenderingDevice, extracted source substitute, or fabricated authority is used.
