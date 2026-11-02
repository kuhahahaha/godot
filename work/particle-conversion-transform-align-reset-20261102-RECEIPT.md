# CPU-to-GPU 3D transform alignment reset receipt

Date: 2026-11-02. Base before this change: `df814bd60f`.

CPUParticles3D has no transform-alignment mode. Converting it into a reused GPUParticles3D target left the target's old billboard or velocity alignment active, changing particle orientation. Conversion now restores the GPU default, disabled alignment.

The headless scene fixture set the reused target to Z billboard and failed before the fix with `CPU to GPU 3D conversion retained old transform alignment` (exit 1). After the one-line change, `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree. The rebuilt binary SHA256 is `dfb468215ba904ce5cbf1df0514dfa7c0e2d56cc879e946b248d03f19e60a943`; the fixture then exited 0. This verifies scene state under dummy rendering, not live GPU orientation or an isolated build of this commit.
