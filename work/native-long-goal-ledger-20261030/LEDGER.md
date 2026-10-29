# Native long-goal ledger — 2026-10-30

The current tests-enabled editor is pinned to SHA `fb8693bd5c159a724793b17f23581b1fdc2e1ba9875c0195834883ec79cc736a` and its build returned rc0. K03, Metal, Vulkan, MetalCompletion, ParticlesReadback, E05, QualificationRt, and TypedABI selectors all passed; details are in `work/current-editor-tests-build-20261030/RECEIPT.md`.

The exact non-headless runtime pair is public editor SHA `150c8f1234b6cfd45ad163ed82f8cb50ef6e76d7016ee4c918e56489f1a43162` with stable DSO SHA `0e1750c7ef509096201c849116d28ae385bd429ee914e4adebbd8960f0284041`. All three fixtures load the extension and stop at `host.storage_owner_transport / E05 / UNAVAILABLE`; no world admission follows.

The remaining production seam is external owner input. E05 transport/account data, E06 issuer, and R01/D completion/readback/disposal authority are absent. The native substrate work continues independently and does not promote these negative runtime results to issuer, completion, Ready, or G1 authority.

CI run `40930940598` remains in progress only for the Linux doubles GCC sanitizer job; it is tracked separately from the local green evidence.
