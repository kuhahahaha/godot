# K03 ANGE VFX projection consumer census — 2026-10-27

## Finding

The fixed-pair VFX source stage now compiles the system file and produces a
validated `SystemRegistryProjection`, but the current ANGE runtime does not
consume that projection as a producer-owned particle operation. This is a
precise source boundary, not a runtime failure and not permission to infer GPU
execution from validation.

## Actual call chain

1. `ff_vfx.cpp` routes scanned `.vfx` inputs to
   `WorldLoader::load_vfx_sources()`.
2. `WorldLoader::load_vfx_sources()` prepares the VFX models and calls
   `compile_system_bundle()` for each system entry. It retains the bundle and
   projection only when the lease and bundle identity are valid.
3. `EcsWorld::do_world_load()` calls
   `validate_vfx_system_projections()`. That function checks null and semantic
   projection validity and rolls back on invalid data.
4. The visual bridge's `BridgeParticle` observer consumes an ECS `Particle`
   component and calls `create_particles_node(p.data, ...)`. It has no call
   site that consumes `SystemRegistryProjection`, a system bundle lease, or an
   authenticated E09/U08 operation.

The source tree therefore has a real source/semantic validation path and a
separate legacy/inline Particle-to-Godot bridge. The projection is not silently
converted into a Particle component, GPUParticles3D node, native lease, packet,
or completion fact.

## Fixed-pair evidence

The non-headless Metal/mobile VFX probe is positive for the source stage:

```text
[vfx] runtime sources ready entries=2 systems=1
U01_EXTENSION_OK=true
U01_ENTER_WORLD=true
U01_UNLOAD_OK=true
```

The no-VFX control is negative for system projection creation while keeping the
same lifecycle boundary:

```text
[vfx] runtime sources ready entries=2 systems=0
U01_EXTENSION_OK=true
U01_ENTER_WORLD=true
U01_UNLOAD_OK=true
```

These logs prove source loading, projection construction, validation, world
entry, and explicit close status. They do not prove that a particle operation
was issued or that GPU work, packet output, readback, physical disposal,
Ready/Verified, or completion occurred.

## Minimum owner join

The next implementation must be owned by the original producer/operation
boundary. It needs a typed system-to-particle association, same-request
operation/source key, explicit admission/lease result, and a consumer that
returns an authenticated completion/disposal receipt. The bridge must reject
projection-only inputs until those facts exist. A default ECS entity, RID,
ObjectID, bundle pointer, or projection count cannot serve as that authority.

Until the owner join arrives, the correct behavior is successful semantic
projection validation followed by no positive GPU claim. This keeps the VFX
source work moving without weakening G1.

## Source evidence

- ANGE source HEAD: `b2038e9a5be40624f6b19613f209422da49b4355`.
- Dirty `world_loader.cpp` SHA-256:
  `32d1b15304c0dcf38ef731f0cc738f7b018a4c83bec828545f1fca89acac0d6b`.
- `world_loader.h` SHA-256:
  `77f296144672bc0a4d9bcc9415e0f32a9a3614d872e8d20c9e5df0170e0d7445`.
- `ecs_world.cpp` SHA-256:
  `851460f90103af427c273d7b2ec194ef7acf8d975b8e167248e652959be24ddf`.
- `sys_visual_bridge.cpp` SHA-256:
  `93a84a0d702956f3a4debe5e535b4bc6476c29b4815fddfe515b17ac194432bb`.
- `ff_vfx.cpp` SHA-256:
  `e0b8003a3fb3ba94865a74b9ebebfb2270556a00f0d8add4ab3a2e5240e4f7f2`.

No U08/E09 association, operation/completion, readback, packet, physical
disposal, Ready/Verified, or GPU qualification is claimed.
