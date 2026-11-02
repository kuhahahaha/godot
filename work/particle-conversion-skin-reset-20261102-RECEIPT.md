# CPU-to-GPU 3D skin reset receipt

Date: 2026-11-02. Base before this change: `7532f4a4fb`.

CPUParticles3D has no skin property. Converting it into a reused GPUParticles3D target left an old Skin attached; that Skin can override mesh bind poses. Conversion now clears the target Skin after assigning the CPU mesh, using the existing setter so the old resource signal is disconnected and bind poses are refreshed.

The headless scene fixture assigned an old Skin and failed before the fix with `CPU to GPU 3D conversion retained old skin` (exit 1). After the one-line change, `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree. Rebuilt binary SHA256: `4a94ae8c6ebfe1e97d4af87fa680ee70dfe3c1c1bc5610689bd215e2fb91b6b1`. The fixture then exited 0. This verifies scene state under dummy rendering, not live GPU bind poses or an isolated build of this commit.
