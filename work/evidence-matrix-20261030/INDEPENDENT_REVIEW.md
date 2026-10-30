# Independent review — Godot/native evidence matrix

The matrix was assembled from the committed package manifests and receipts listed
in `MATRIX.json`; no runtime result was inferred from a fixture or copied across
editor/DSO pairs. Each row keeps the exact historical Godot/editor/DSO/PCK hashes,
focused counts, command package, first boundary, and claims withheld.

Evidence classes are separated explicitly:

- `production_fix`: source changes already committed in the referenced package,
  with focused regression suites green.
- `no_code_audit` / `no_code_benchmark`: source or performance evidence only; no
  production behavior was changed or promoted.
- `no_code_upstream_gap`: a precise missing owner/caller input, currently E05.
- `negative_e05_owner_boundary`: current pair probes that fail closed before world
  admission and cannot establish native submit, completion, readback, Ready, or GPU.

The canonical current pair is the E05/E09 census row: Godot editor SHA
`37743639ba01d06aa9b43c5c21582a8d4683ecfcd0ec3ab5b9cbf5e4600bb7e6`, DSO SHA
`38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`, and all three
fixed probes return rc2 at `host.storage_owner_transport/E05/UNAVAILABLE`.

Historical rows intentionally retain their own editor hashes; they are not silently
relabelled as the canonical pair. PCK and performance rows preserve their import,
pack, and run return codes, including expected negative E05/PCK runtime outcomes.
No row claims GPU Ready, native owner, same-request completion/readback, or Sprite
packet authority.
