# Exact editor/DSO runtime rerun — 2026-10-30

This rerun uses the exact pinned public editor and stable DSO pair. It executes the real non-headless macOS Metal probe for VFX, no-VFX, and invalid-VFX fixtures.

- `runtime-vfx`: rc 2; first=host.storage_owner_transport/E05/UNAVAILABLE; extension=True; world_marker=False; unload_marker=False
- `runtime-no-vfx`: rc 2; first=host.storage_owner_transport/E05/UNAVAILABLE; extension=True; world_marker=False; unload_marker=False
- `runtime-invalid-vfx`: rc 2; first=host.storage_owner_transport/E05/UNAVAILABLE; extension=True; world_marker=False; unload_marker=False

The observed owner seam is fail-closed. No native submit, completion, same-request readback, Ready, or G1 authority is claimed.
