# E06 macOS linker-flag parser repair — 2026-10-29

This is an independent native build-system slice. The controlled identity
parser treated two real Darwin linker flags as unclosed operands:
`-S` (strip debug information) and `-x` (remove local symbols). Both are
closed flags and carry no operand.

## TDD and verification

- RED S1: 21 unit tests, expected failure on `-S` with
  `unclosed original native linker operand: -S` (`RED_S1.rc=1`).
- RED S2: after accepting `-S`, 21 unit tests, expected failure on `-x`
  (`RED_S2.rc=1`).
- GREEN unit suite: 21/21, rc0 (`GREEN_UNIT.rc`).
- Production fix: add `-S` and `-x` to `resolve_link_inputs()` closed flags.
- Release build command:
  `/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=template_release dev_build=no tests=no builtin_glslang=yes`
- Release build: rc0 (`RELEASE_BUILD.rc`), 10:05.71 elapsed in the raw log.
- Release executable SHA256:
  `5f7d8191ecb850fe8109a982afba8a11fd6f0cbd6044f64007b2fdd0984663c7`
- Release `--version` and `--help`: both rc0.
- `git diff --check` on parser/test changes: rc0.

The prior release failure at `-S` and the subsequent `-x` failure are retained
as red evidence. This closes the parser branch only; it does not claim GPU,
particle qualification, completion, readback, Ready or G1.
