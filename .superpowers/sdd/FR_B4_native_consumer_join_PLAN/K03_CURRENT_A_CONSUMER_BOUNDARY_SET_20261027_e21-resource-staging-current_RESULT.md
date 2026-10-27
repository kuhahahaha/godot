# E21 resource snapshot staging current-A consumer — 2026-10-27

The six source/helper/test identities are clean and hash-pinned at `173f69aa54ad5272fef8d2e79cdc21152dfec968`. Debug, Release (`-DNDEBUG`) and UBSan standalone variants each compile and run successfully; the Release warning exposed by `NDEBUG` was fixed with a test-only `[[maybe_unused]]` annotation.

This validates only bounded gradient/curve staging against the pure curve provider. E20 owner admission, D/core exports, native/GPU behavior and Ready remain open.
