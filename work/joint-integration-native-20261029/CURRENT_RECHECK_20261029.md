# Current mainline focused recheck

Executed against the current local `codex/particle-mainline` editor binary after the long-goal audit manifest commit.

- Qualification command: `bin/godot.macos.editor.dev.arm64 --headless --test '--test-case=[Qualification]*'`
  - rc `0`; 29 cases / 478 assertions passed.
- Frame-wait command: `bin/godot.macos.editor.dev.arm64 --headless --test '--test-case=*FrameWait*'`
  - rc `0`; 17 cases / 301 assertions passed.

The logs were captured in `/tmp/k03-current-qualification.log` and `/tmp/k03-current-framewait.log`. This is a focused regression recheck only; it does not promote E05/U08, E09, Ready or G1, and it does not change the existing owner boundary.
