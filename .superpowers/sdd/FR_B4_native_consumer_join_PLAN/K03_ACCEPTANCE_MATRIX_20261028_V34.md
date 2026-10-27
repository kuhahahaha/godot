# K03 acceptance matrix v34 — 2026-10-28

This successor matrix preserves v33 and adds the tracked Object lifetime regression test handoff. The test closes a concrete native test-delivery gap; it does not close the broader Node/C4, producer/owner, operation, completion, readback, Ready, Verified, or GPU requirements.

| Package | Commit | Verified scope | Remaining boundary |
|---|---|---|---|
| Godot Object lifetime authority tests | `e2286c8ac8` | Tracked `tests/core/object/test_object_lifetime.cpp`; macOS headless run rc0, 9 cases / 51 assertions | Node ancestor adoption, signal lock/target argument edges, cross-thread lifetime, queue/reload ownership, operation/completion/readback/Ready/Verified/GPU remain open |

The v33 fixed editor/DSO pair and all authority claims remain unchanged.
