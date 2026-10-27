# ANGE current DSO/editor pair rebuild and runtime probes — 2026-10-27

The current ANGE source was rebuilt and signed, then paired with a freshly built Godot dev editor. The actual DSO passes `codesign --verify --deep --strict`.

- Godot editor SHA-256: `bbe896df5fa85f96af16a7ca94d7737641a861fb082e8b3d47606d7a2fef3be8`.
- ANGE DSO SHA-256: `b672d4148aba2cbd40c847d8125732d9fa84b2f217bfeb05a0802f9143cb492d`.
- SCons rebuild: rc0; codesign verification: rc0.
- Non-headless Metal/mobile probes: VFX rc0 (entries=2, systems=1), no-VFX rc0, explicit close rc0 (active true→false), malformed VFX rc1 with `VFX-SYN-001`, rollback, and unload success.

This validates the current binary pair's extension/world/unload and fail-closed source rollback behavior. It does not provide U08/E09 producer association, operation/completion/readback authority, packet publication, physical disposal, Ready, Verified, or GPU qualification.

Evidence: `K03_ANGE_CURRENT_DSO_PAIR_REBUILD_20261027_REPORT.json` and `/tmp/k03-current-dso-canonical-20261027/`.
