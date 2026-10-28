# Independent review — current nonbuiltin pair

Scope is limited to the three real non-headless Metal controls recorded in
`RECEIPT.md`.

- The host and DSO SHA pins are explicit and the raw logs show Metal device
  startup before the extension boundary.
- All three controls return rc2 with `U01_EXTENSION=1`; the first shared error
  is `install_main_storage`, code 13,
  `host.storage_owner_transport/E05`, expected 1 and actual 0.
- No fixture reaches world admission, VFX source loading, unload, native issue,
  completion, readback or Ready. The invalid-VFX control is therefore not an
  independent source-rejection result in this pair.
- No local fallback, synthetic owner, RID/ObjectID inference or G1 relaxation
  was introduced.

Disposition: precise external owner boundary, no P1/P2 finding in this
bounded runtime review. This does not close U01 or any native qualification
gate.
