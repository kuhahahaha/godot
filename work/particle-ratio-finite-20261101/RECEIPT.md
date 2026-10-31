# Particle ratio finite-input receipt

## Scope

Guard the native RD particle setters for amount ratio, explosiveness ratio, and randomness ratio against non-finite floating-point input. Invalid values are rejected before they can enter frame parameters or the GPU process path; the previous value remains unchanged by the existing `ERR_FAIL_COND_MSG` setter behavior.

## Changed files

- `servers/rendering/renderer_rd/storage_rd/particles_storage.h`
- `servers/rendering/renderer_rd/storage_rd/particles_storage.cpp`
- `tests/servers/rendering/test_particles_readback.cpp`

## Verification

- `scons platform=macos target=editor tests=yes arch=arm64 -j3` — passed.
- `bin/godot.macos.editor.arm64 --headless --test --test-case='[ParticlesReadback] particle ratios reject non-finite values' --quit` — 1 case, 7 assertions passed.
- `git diff --check` — passed.

The repository-wide clang-format dry run still reports pre-existing formatting violations in unrelated regions of `particles_storage.cpp` and the existing include ordering in `test_particles_readback.cpp`; this patch does not reformat those lines.

## Boundaries

This receipt covers input validation only. It does not claim production Metal retry execution, same-request completion/readback, physical retirement, or typed-close behavior.
