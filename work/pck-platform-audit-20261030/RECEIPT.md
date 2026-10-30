# Minimal particle PCK/platform loading audit — 2026-10-30

## Scope and outcome

A minimal fixture was copied from the current no-VFX pair, bound to the explicit current ANGE DSO, and given a macOS PCK export preset. Editor import completed rc0, PCK export completed rc0, and the resulting PCK loaded far enough to initialize Metal and the extension. Both source-path and PCK-path runtime then stopped at the same guarded E05 owner boundary with rc1. No Godot loader/export production defect was found; the only export warning is the expected missing x86_64 GDExtension library for this arm64-only DSO. The fixture adds a `macos.arm64` key for platform packaging; no production source was changed.

## Pair and artifact identity

- Godot source HEAD: `6a72cd0c94a91a10eebab8cc521deb846a928ec1`.
- Editor SHA-256: `b39543b6c5b8d96e3acabae8b3212710cc60ffc0248fa4f8261eab9491bb1be9`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- Explicit DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.
- PCK: `particle-audit.pck`, 2,188 bytes, SHA-256 `35dbfdafebb916de15ca67004da811ee0aa05ebaf0e8b7bf3787f038cd0f0645`.

## Commands and results

Resource import:

```text
bin/godot.macos.editor.dev.arm64 --headless --editor --path work/pck-platform-audit-20261030/fixture --import --quit
```

rc 0. The editor scanned the project, validated the GDExtension and generated resource caches.

PCK export:

```text
bin/godot.macos.editor.dev.arm64 --headless --path work/pck-platform-audit-20261030/fixture --export-pack 'PCK Audit macOS' particle-audit.pck
```

rc 0. Export logged one warning: no `x86_64` GDExtension library exists. The arm64 key resolves to the explicit DSO; x86_64 packaging is intentionally unavailable.

Source fixture probe:

```text
bin/godot.macos.editor.dev.arm64 --headless --path work/pck-platform-audit-20261030/fixture --display-driver macos --rendering-driver metal --rendering-method mobile --render-thread safe --script res://probe.gd
```

rc 1. PCK probe:

```text
bin/godot.macos.editor.dev.arm64 --headless --main-pack work/pck-platform-audit-20261030/particle-audit.pck --display-driver macos --rendering-driver metal --rendering-method mobile --render-thread safe --script res://probe.gd
```

rc 1. Both produce Metal 4.0 startup and the same first boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
ERROR: SceneReady materialization failed (2).
```

The script does not reach `U01_EXTENSION`/world admission because SceneReady is refused before probe execution. This is an upstream E05 owner transport boundary, not a PCK parse or GDExtension load failure. No native completion/readback, visible output, Ready, or GPU claim is made.
