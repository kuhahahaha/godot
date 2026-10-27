# Native Object lifetime regression receipt

Date: 2026-10-28

## Scope

This package lands the reviewed `tests/core/object/test_object_lifetime.cpp` handoff for the Object-owned deletion gate and native continuation registration. The production authority is already present in `core/object/object.cpp/.h` from the reviewed baseline; this commit adds the real regression translation unit to the tracked tests tree.

Covered behaviors include nested PREDELETE refusal, cancellation gate reset, registration invalidation before cleanup, intrusive registration removal in both directions, stack Object base destruction, emitter deletion during signal delivery, and nested emission state restoration. The tests use real `memdelete`, `cancel_free`, `Object::NativeContinuation`, and signal delivery.

This package does not claim complete C4/Skin closure, Node ancestor adoption, cross-thread lifetime pinning, queue retirement, reload ownership, native operation, completion, readback, Ready, Verified, or GPU qualification.

## Verification

- Commit: `e2286c8ac8` (`Add native Object lifetime regression tests`).
- Existing rebuilt editor binary: `01660f9105c2bfbd869f2e0d34afba8f54e62dd7403cc8a5efa9a248b8de9348`.
- Command: `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='[Object][Native lifetime]*'`.
- Result: rc0; 9 test cases / 51 assertions passed, 0 failed.
- Two freed-during-emission diagnostics are expected witnesses from the tested warning path.
- `git diff --check` passed for the committed test source before commit.

No ASan, release, GPU, cross-thread, Android, or non-macOS runtime claim is made.

Raw log: `native-lifetime.log`.
