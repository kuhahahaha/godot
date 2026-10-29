# External build blocker — R46

The current mixed worktree contains an unrelated uncommitted interface addition `RendererParticlesStorage::particles_get_current_data` in `servers/rendering/storage/particles_storage.h` and a new `tests/servers/rendering/test_particles_readback.cpp`. The existing GLES3 `ParticlesStorage` implementation has not yet implemented that pure virtual.

The first UBSan build failed while reading the transient older header (`test_particles_readback.cpp:12`); after the concurrent header update, the retry reached `drivers/gles3/rasterizer_gles3.cpp:374` and failed because `GLES3::ParticlesStorage` remains abstract. The R46 Vulkan source was not reverted or hidden, and no stale sanitizer binary is counted as current-source evidence.
