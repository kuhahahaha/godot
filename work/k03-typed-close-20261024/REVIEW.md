# Q-D-K03 typed-close/recreate/cleanup review — 2026-10-24

Scope was limited to the existing dirty-tree K03 implementation in `core/extension/gdextension_library_exit_guard.{h,cpp}`, `core/object/object.{h,cpp}`, and `tests/core/object/test_device_exit_entry.cpp`.

Static review confirms the Registry keeps typed cleanup actions in phase-partitioned counters, reserves cleanup identities before quiesce, rejects late/unissued phase actions, and computes `physical_can_close_locked` only after cleanup reservations, active/retiring cleanup, callbacks, holders, and qualified level state are clear. `CleanupScope` retains the holder through callback return and records core disposal before retirement. Object recreation reserves and rolls back cleanup state before publication, and the existing recreate/closed-refusal path exercises the seam.

Focused verification used the existing `bin/godot.macos.editor.arm64` binary:

- `device_exit::object/recreate-frame-and-closed-refusal`: exit 0, 1 passed, 18 assertions.
- `device_exit::entry/cleanup-capacity-and-detached-scope`: exit 0, 1 passed, 19 assertions.

Receipt: `FOCUSED_NATIVE_CHECK.json`.

Disposition: **APPROVE_LIMITED for the reviewed K03 typed-close/recreate/cleanup seam.** This does not qualify the full native package, guarded public activation, U04/world/service issuer, GPU, or production readiness. A fresh full rebuild remains blocked by `No space left on device`; focused runs used an existing binary.
