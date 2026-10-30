# Native texture import bool-contract regression coverage — 2026-10-30

This package follows production commit `f4b839b4a08de55ac6062cbe5970464b36a60692`.
It adds focused negative/source-contract coverage for the new
`texture_create_from_native_handle()` failure contract without changing production
source again.

`CONTRACT_CHECK.py` checks the common, RD, GLES3, and dummy bool overrides, invalid
source/format/type/usage guards, primary and sRGB shared-view failure checks,
reserved RID cleanup, source RD RID cleanup, and success publication ordering. It
returned rc 0 with every check true.

A runtime backend-fault test could not be injected safely in the current harness:
`TextureStorage` construction requires a live RenderingDevice/MaterialStorage stack,
and the available fake drivers exercise `RenderingDevice` directly rather than the
renderer storage import wrapper. Therefore no synthetic driver failure or fabricated
cleanup count is claimed. The static contract check is supplemental evidence only;
the production fix's compile and focused behavior remain covered by the prior package.

## Fresh verification

- Editor Debug module-on rebuild: rc 0 (`EDITOR_BUILD.log`), editor SHA
  `37743639ba01d06aa9b43c5c21582a8d4683ecfcd0ec3ab5b9cbf5e4600bb7e6`.
- Contract check: rc 0 (`CONTRACT_CHECK.log`).
- RenderingDevice: rc 0, 12/12 cases, 214/214 assertions.
- Qualification: rc 0, 102/102 cases, 1896/1896 assertions.
- K03: rc 0, 44/44 cases, 1153/1153 assertions.
- R01: rc 0, 16/16 cases, 293/293 assertions.

The explicit current Metal pair still fails closed at E05 owner transport for VFX,
no-VFX, and invalid-VFX (all rc 2). No operation, completion, readback, visible
output, Ready, or GPU claim is made.
