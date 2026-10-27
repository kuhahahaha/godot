# E17/E18 scene geometry current-A consumer — 2026-10-27

The five source/helper/test identities are clean and hash-pinned at `d8dc4d29ef8300e4f5a1b71e818c948bf3c219d0`. Debug, Release (`-DNDEBUG`) and UBSan standalone Catch2 variants each pass 3 cases with 966 assertions and zero failures.

This is control-side geometry preparation only. It does not confer sampling/phase ownership, E07 allocation, runtime/native/GPU qualification or Ready.
