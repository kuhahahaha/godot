# P01 native observer / fixture / retirement boundary — 2026-10-30

This package is a no-code, source-backed Godot audit. The current Godot source is
`d0dbe361ae10a451fb2932b855bb362a3e14e0ff`; the loaded ANGE DSO is
`38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`. The current
ANGE checkout at audit time is `1d9aeef2561a037cf785d9d602a45e321eb7539f` and is
not modified by this package.

The P01 observer and fixture path is explicitly test-only. All observer declarations
are gated by `TESTS_ENABLED && MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED`, and the
retirement pump is called only in that gated path. The fixture maintains exact
allocation/readback identities and waits for callback holders, logical release, and
physical retirement before terminal removal. Poisoned evidence closes fail-closed;
no fallback owner or completion is introduced.

## Build and focused regressions

- Module-off editor build: `EDITOR_BUILD.rc=0` (`EDITOR_BUILD.log`); the module-off
  P01 selector reports `0` selected / `0` failed, as expected.
- Module-on editor build: `EDITOR_BUILD_MODULE.rc=0` (`EDITOR_BUILD_MODULE.log`);
  editor SHA256 is `65b76f85b6c0b3d0c2b0ef38eb713643a4910680afed901689b0a076c87c4a6f` (`EDITOR_SHA256_FINAL.txt`).
- P01: rc `0`, `91/91` cases, `9612/9612` assertions (`P01.log`).
- RenderingDevice: rc `0`, `12/12`, `214/214` (`RenderingDevice.log`).
- Qualification: rc `0`, `102/102`, `1896/1896` (`Qualification.log`).
- K03: rc `0`, `44/44`, `1153/1153` (`K03.log`).
- R01: rc `0`, `16/16`, `293/293` (`R01.log`).
- CallQueue: rc `0`, `6/6`, `92/92` (`CallQueue.log`).

No ASAN editor was available in this checkout for a fresh module-on run; no ASAN
result is claimed. Existing ASAN artifacts were not reused as current-pair proof.

## Current Metal pair

Using the explicit DSO path recorded in `DSO_SHA256.txt`, all three fixed fixtures
return rc `2` and fail at the same first boundary:

```
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

`runtime-vfx.log`, `runtime-no-vfx.log`, and `runtime-invalid-vfx.log` also contain
non-authoritative shader-cache directory warnings from the fixture user-data path.
No `U01_ENTER_WORLD`, operation, completion, readback, Ready, or GPU-visible result
is reached. The invalid-VFX fixture is stopped by E05 before source validation.

This evidence does not establish a native owner, physical GPU completion, P01/GPU
Ready, or fallback behavior. The remaining gap is the upstream E05 owner transport
and a production typed packet/observer contract; this Godot slice cannot close it
without inventing authority.

See `SOURCE_AUDIT.txt`, `INDEPENDENT_REVIEW.md`, and `MANIFEST.json` for the exact
source locations, hashes, commands, and excluded dirty baseline.
