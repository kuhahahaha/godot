# CPU-to-GPU directed points without normals receipt

Date: 2026-11-02. Base before this change: `56c92ad2e3`.

CPUParticles2D/3D accept DIRECTED_POINTS with a missing or wrong-length normal array and skip normal rotation. ParticleProcessMaterial's DIRECTED_POINTS shader always fetches a normal texture. CPU-to-GPU conversion now selects ordinary POINTS when the source normal count differs from the point count, retaining the encoded positions without enabling the unavailable normal fetch. Matching normal arrays retain DIRECTED_POINTS and their encoded normal texture.

The headless fixture failed before implementation in both dimensions (exit 1). After the change it passed (exit 0) for empty and mismatched normal arrays. The previous CPU point-array roundtrip fixture also passed. `scons tests=yes platform=macos target=editor -j4` completed successfully on the mixed worktree, and existing conversion doctests passed 11 cases / 59 assertions. Rebuilt binary SHA256: `bd51a1998eaffee6bf8e2dc9d902b78b42d4330787732417c2bafb53c90a6a53`. These checks verify scene state under dummy rendering, not live GPU emission or an isolated build of this commit.
