# Vulkan queue-submit view validation — R31 — 2026-10-30

`command_queue_execute_and_present()` now rejects nonempty wait-semaphore, command-buffer, command-semaphore, or swap-chain views with null backing pointers before queue identity assertions and state mutation. The prior malformed wait view reached the queue assertion and aborted. This is fail-closed native input validation; valid queue submission and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED against the pre-fix binary: null wait view reached the old queue assertion and aborted, rc134.
- Debug GREEN: focused 1 case / 3 assertions, Vulkan qualification regression 32 cases / 122 assertions, K03/P2 22 cases / 350 assertions, all rc0.
- Actual `use_ubsan=yes`: Vulkan qualification 32/122, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.742e36883`). Source diff check rc0.

Godot source HEAD at capture is `14d5824da30616e5cfcc28a8271437454491d8e7`; implementation commit is `14d5824da3`. This is bounded native input validation and does not close the separate owner or K03 operation/completion/readback joins.
