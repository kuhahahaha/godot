# K03 acceptance matrix v30 receipt — 2026-10-28

- Preserves matrix v29 and adds the C04 production ordering/allocator contract fix.
- Opt-in `test-c04-spirv` now builds rc0 and runs 66 cases / 6,985 assertions with rc0 and zero failures.
- Production change is limited to deterministic accessed-variable ordering; test instrumentation targets the real charged malloc path.
- Default graph remains opt-in; no native/GPU/owner/Ready claim.
