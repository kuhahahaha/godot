# Collision-size fixture restoration

Source baseline: `c60fdba4e4`. Production setters are unchanged.

The extracted RD setter now reads `Particles::external`. The fixture substitute lacked that member, so its C++ harness failed compilation before checking collision sizes. Add the corresponding nullable owner pointer and verify that a valid size update leaves state unchanged while an external owner is present.

Validation on 2026-11-06:

- `python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py`: exit 0; 7/7 behavior checks and 4/4 source checks.
- `python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py --source-ref HEAD`: exit 0; 4/4 source checks.
- In-memory mutation removing only the external-owner guard from the extracted collision-size setter: harness exit 1, exactly the external-update assertion failed (6/7). No production source was modified for this control.
- Existing `bin/godot.macos.editor.arm64 --headless --test --source-file='*test_gpu_particles.cpp' --quit`: 18/18 cases, 90/90 assertions. This binary predates the fixture change; this change is standalone Python/extracted C++, not compiled into the editor.

These checks cover numeric setter state and external mutation rejection only. They do not prove real GPU execution or Production raster/Sprite lifecycle acceptance. Full editor rebuild remains unverified after prior disk-capacity failures.
