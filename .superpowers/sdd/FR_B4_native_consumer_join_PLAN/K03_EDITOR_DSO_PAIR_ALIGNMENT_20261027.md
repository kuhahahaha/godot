# K03 editor/DSO exact pair alignment — 2026-10-27

## Build alignment

- Godot source commit: `60b0cd48d037ea176ee8c805361c5dcadd49b099`
- Godot editor build: rc=0, SHA-256 `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853`
- ANGE source HEAD: `695921a207d89b27da6687453eba6a4ff2ec383d`
- ANGE DSO build: rc=0, SHA-256 `a087a9054c1079efdec85a91759647bea37aa46a0b1fd4d09a7b26ed4b9713e6`
- Renderer: non-headless Metal/mobile, Apple M3 Pro, render-thread safe.

The editor was rebuilt from the source containing the v2 scene-ready interface;
the DSO was rebuilt after the current ANGE source drift. No interface fallback
or ABI weakening was added.

## Fresh fixed-pair probes

VFX fixture:

```text
U01_EXTENSION_STATUS=0
U01_EXTENSION_OK=true
[vfx] runtime sources ready entries=2 systems=1
U01_ENTER_WORLD=true
U01_UNLOAD_STATUS=0
U01_UNLOAD_OK=true
```

No-VFX emitter control:

```text
U01_EXTENSION_STATUS=0
U01_EXTENSION_OK=true
[vfx] runtime sources ready entries=1 systems=0
U01_ENTER_WORLD=true
U01_UNLOAD_STATUS=0
U01_UNLOAD_OK=true
```

Both probes return rc=0. The logs contain non-fatal tree/flower authoring
default warnings; no extension, world, or close error occurs.

This proves exact host/DSO interface alignment, source-stage VFX projection,
world entry, and explicit unload. It does not prove U08/E09 association,
operation/completion, readback bytes, packet publication, physical disposal,
Ready/Verified, or GPU qualification.
