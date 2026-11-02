# GPU-to-CPU nonfinite normal texture receipt

Date: 2026-11-02. Base before this change: `a3e020f3e5`.

GPU-to-CPU point conversion accepted finite position texels, then sent all decoded normal texels to the CPU setter. An INF/NaN normal caused the setter to reject the array and log an error. Both 2D and 3D paths now validate the full normal array before calling the setter. Valid positions remain; invalid normals leave the optional CPU normal array empty, matching the CPU simulation's handling of unavailable normals.

The headless fixture uses finite positions and INF/NaN normal texels in 2D/3D. Its Python runner failed before implementation on the CPU setter error and passed after implementation, asserting that positions remain and no setter error is emitted. The valid point-texture fixture passed. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree; conversion doctests passed 11 cases / 59 assertions. Rebuilt binary SHA256 `08cdfe891dc6ef66d602b1d544534c657c38571c683671ea4f401dff1bddc001`. These checks verify scene data under dummy rendering, not live GPU emission or an isolated build of this commit.
