# ANGE native reader/diagnostic regression — 2026-10-28

Against ANGE source `bba55f3ba` and DSO `38c3da7e…`, the focused native boundary tests passed:

- `E05 native readers`: 1 case / 12 assertions
- `E06 external diagnostics`: 1 case / 13 assertions

This is CPU/in-process reader and diagnostic coverage. It does not provide an original producer, operation/completion/readback authority, Ready, Verified, or GPU qualification.
