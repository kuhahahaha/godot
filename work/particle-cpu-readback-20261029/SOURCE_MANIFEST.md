# Particle CPU readback source manifest — 2026-10-29

## Scope

This change adds a real `ParticlesStorage -> RenderingServer` CPU readback
entry for the latest particle storage buffer. It is keyed by the caller's real
particle `RID`; no synthetic owner or test owner is introduced. RD returns the
full process `ParticleData` storage, GLES3 returns its current instance buffer,
and dummy storage fails closed with an empty byte array.

## Sources

| File | Role | SHA-256 |
|---|---|---|
| `servers/rendering/storage/particles_storage.h` | storage contract | `6f88499ac0eefe164cff2a06969755ef4093a673125ab3f187149df752603300` |
| `servers/rendering/dummy/storage/particles_storage.h` | dummy fail-closed implementation | `2b74f324e44407b9b3e5aa65c3cf5568c970af490bfec6b9d0792522c4bbb4c2` |
| `servers/rendering/renderer_rd/storage_rd/particles_storage.h` | RD declaration and test access | `2dfaa1545d333840b01300a1f9baadc6ed0ea3a99e58c2bc4e5350049a2fc145` |
| `servers/rendering/renderer_rd/storage_rd/particles_storage.cpp` | bounded GPU readback and layout sizing | `d8b884f17359bac057e7b2905bc4780a1cb36d02985ffd57a3e44e7a6a0fef81` |
| `drivers/gles3/storage/particles_storage.h` | GLES3 declaration | `76e8ccd881b86b1b07ab9279b5c0a9282ad85346e96d33bb6e88689c60f796af` |
| `drivers/gles3/storage/particles_storage.cpp` | GLES3 buffer readback | `66a371f0afee9647d72fb22ec9e663df15197d41dbeedbe32c3d3d39cb9df53d` |
| `servers/rendering/rendering_server.h` | public RenderingServer API | `428a74fe79942c85cbacfa8a76005478f902981a28113220670db99e68a639d3` |
| `servers/rendering/rendering_server_default.h` | render-thread forwarding | `7bbcc09247f13237031b657588b8a4351625da6534c6b7c68180a895e70c699a` |
| `servers/rendering/rendering_server.cpp` | script/native method binding | `20466b5a12a286064b1a340d27ee0b4423c8a06bc484380aae9fd9c0214f00e6` |
| `tests/servers/rendering/test_particles_readback.cpp` | layout and fail-closed regression | `085ee9604d29001f3917ea88c8e0f202704591ae97754de2c622e437442e5e97` |
