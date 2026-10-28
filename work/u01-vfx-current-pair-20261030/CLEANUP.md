# Cleanup receipt — 2026-10-30

- The three Godot processes exited with recorded return code `2`.
- `pgrep -x godot.macos.editor.dev.arm64` and `pgrep -x Godot` returned no process after the run.
- Only the new evidence directory `work/u01-vfx-current-pair-20261030/` was created or written by this task; formal Godot/ANGE source files and pre-existing dirty content were not reset or cleaned.
- Copied fixtures are evidence inputs. Their DSO path remains the current absolute ANGE DSO path recorded in `RECEIPT.json`.
