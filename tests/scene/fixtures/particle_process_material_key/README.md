# Particle process-material shader-key retirement regression

`ParticleProcessMaterial::MaterialKey` previously had 46 named bits and 18 padding bits. Hash/equality covered all eight bytes, while implicit assignment could leave padding different from the computed/cached key. The observed computed/cached key was `0000200001000001`; the corresponding material key at destruction was `0000200001000000`. Both shared materials then missed the cache entry and failed to decrement its users. The trace does not establish where the nonzero padding originated.

The repair makes all 64 bits named (`reserved:18`) and adds size and unique-object-representation assertions. It preserves the existing key fields, hash and cache ownership.

The engine tests use public material/node operations and the dummy storage owner to verify sharing, repeated queries, variant replacement, survival after the first release and retirement after the last release. Original code builds, then both tests fail at three final-retirement assertions and report three shader RID leaks. Fixed code passes 2 tests / 18 assertions. The complete relevant CPU selector passes 41 tests / 1315 assertions, exits 0, and reports no RID leak. Logs and exact artifact/source identity are in `RECEIPT.json` and adjacent files.

```sh
GODOT_BIN --test '--test-case=[ParticleProcessMaterial]*' --no-skip=true
GODOT_BIN --test '--test-case=[ParticlesReadback]*,[ParticlesProcessRetry]*,[ParticlesTrail]*,[GPUParticles]*,[CPUParticles]*,[Particles]*,[ParticleProcessMaterial]*' --no-skip=true
```

These are dummy-renderer lifecycle checks. No real GPU acceptance or full Godot suite result is claimed.
