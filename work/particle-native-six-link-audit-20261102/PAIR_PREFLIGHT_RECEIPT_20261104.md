# Owner package pair preflight — 2026-11-04

The three-scenario owner package preflight now reads adjacent controlled-build identity sidecars when both the pinned editor and DSO provide them. It compares core stamp, compiler route/stamp, platform, architecture, and target kind before launching any scenario. A mismatch is rejected with `editor/dso controlled-build identity mismatch`.

This prevents the current fixed Godot editor (`582349c6...` / `e802f348...`) from being paired with the retained native-test DSO (`1da8f740...` / `39a3123a...`). Packages without sidecars retain the existing unit-test behavior.

Verification:

- `python3 -m py_compile work/particle-native-six-link-audit-20261102/run_owner_package.py` — pass.
- `python3 -m unittest work/particle-native-six-link-audit-20261102/test_owner_package.py` — 10 tests, pass.
- Synthetic editor/DSO sidecars with different core stamps — rejected before launch.
- `git diff --check -- work/particle-native-six-link-audit-20261102/run_owner_package.py` — pass.

This is a preflight integrity guard; it does not claim the System/Method six-link product gate is passed.
