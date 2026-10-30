# Independent review — P01 native observer / fixture / retirement

- Reviewed `servers/rendering/render_capture_test_hooks.h`, `servers/rendering/SCsub`,
  `servers/rendering/rendering_server_default.cpp`, the P01 native session, observer,
  collector, and allocation registry sources. The observer APIs and retirement pump
  are test-module gated; no production owner, completion, or packet adapter is present.
- The native session closes on poisoned transport/observer evidence, drains exact
  callback holders, closes collectors, releases the pool, and checks physical
  retirement before removing the session. This ordering is fail-closed and safe to
  retain.
- No source change was made because a production observer or synthetic owner would
  cross the E05 authority boundary. Existing fixture tests remain test-only.
- Module-on editor build and P01/RenderingDevice/Qualification/K03/R01/CallQueue
  focused suites are green with the counts in `RECEIPT.md`.
- Current Metal pair remains negative at E05 owner transport for VFX, no-VFX, and
  invalid-VFX. No GPU Ready or completion claim is made.
- External `godot-cpp` is not a pinned input in this Godot repository; no ABI/DSO
  compatibility claim is made beyond the recorded explicit DSO hash.
