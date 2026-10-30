# Pipeline/shader retirement current audit — 2026-10-30

No production source changed in this package. The current source and binary are
pinned in `GODOT_HEAD.txt`, `EDITOR_SHA256.txt`, and `DSO_SHA256.txt`.

Fresh focused selectors on the same editor:

- `[QualificationNative][Metal]*`: rc0, 2/2 cases, 51/51 assertions.
- `[K03][MetalCompletion]*`: rc0, 6/6 cases, 386/386 assertions.
- `*[Qualification]*unknown pipeline*`: rc0, 1/1 case, 30/30 assertions.
- `*[Qualification]*retire*`: rc0, 14/14 cases, 311/311 assertions.

The three current Metal pair probes each return rc2 at
`host.storage_owner_transport/E05/UNAVAILABLE`; that negative boundary precedes
shader/pipeline work. No owner, native submit, completion, readback, Ready, or GPU
claim is made.
