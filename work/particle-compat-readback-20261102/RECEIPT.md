# Compatibility particle readback runtime receipt

Date: 2026-11-02

A fresh `scons tests=yes platform=macos target=editor -j4` build completed with rc 0. The resulting `bin/godot.macos.editor.arm64` SHA256 is `e8ee19ff9f59f08bf6ccd1b5767c040c52732455a340b88b0da6027ad332e162`. This build used the current mixed working tree (HEAD `362228d502` plus unrelated dirty changes), so it verifies the integrated current state rather than an isolated clean commit.

`python3 work/particle-compat-readback-20261102/run.py` passed all seven checks under OpenGL 4.1 Metal Compatibility: initial 2D rect and 3D AABB were empty; after 60 frames the 2D rect had positive X/Y spread and 3D AABB had positive X/Y/Z spread; both nodes were freed; no engine or script errors appeared; process rc was 0. `RESULTS.json` records the command, binary hash, and observed sizes. The existing 230-frame Compatibility lifecycle smoke also passed all eight checks on this binary (active/restarted/resumed 690 red pixels; stopped/freed 0). `[ParticlesReadback]*` passed 11 cases / 58 assertions.

The probe covers readback behavior through public `capture_rect()` and `capture_aabb()` with a live Compatibility renderer. It does not directly invoke `particles_get_current_data()` or inject a GPU buffer allocation failure; those branches retain source-level evidence only.

## Direct current-data extension

The probe now creates a direct particle RID with amount 4 before any buffer allocation and calls `RenderingServer.particles_get_current_data()` on the live OpenGL3 Compatibility renderer. It observed zero bytes and freed the RID. The extended runner passes 8/8 checks on the same binary SHA256 above. This covers the unallocated current-data branch; a forced allocation-failure retry remains unverified.
