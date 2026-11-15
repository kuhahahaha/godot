# Particle setter dependency receipt

- Commit: `caf4663bdf`, `be313c85d5`, `4089e69081`, `499b1e83a9`, `0550a9be7f`
- Scope: mode, draw-pass count, draw-pass mesh, and draw-order changes notify `DEPENDENCY_CHANGED_PARTICLES` after valid updates; invalid setter inputs remain rejected.
- Verification: `python3 tests/servers/rendering/fixtures/particle_storage_setter_bounds/run.py` → `34/34`; `python3 tests/servers/rendering/fixtures/particle_gles3_draw_pass_count/run.py` → `4/4`; `python3 -m py_compile tests/servers/rendering/fixtures/particle_storage_setter_bounds/run.py`; `git diff --check`.
- Limitation: this fixture verifies the setter/dependency contract only; it does not claim GPU execution or native external-output source/Host association.
