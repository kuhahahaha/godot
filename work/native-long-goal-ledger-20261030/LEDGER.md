# Native long-goal ledger — 2026-10-30

The current tests-enabled editor is pinned to SHA `fb8693bd5c159a724793b17f23581b1fdc2e1ba9875c0195834883ec79cc736a` and its build returned rc0. K03, Metal, Vulkan, MetalCompletion, ParticlesReadback, E05, QualificationRt, and TypedABI selectors all passed; details are in `work/current-editor-tests-build-20261030/RECEIPT.md`.

The exact non-headless runtime pair is public editor SHA `150c8f1234b6cfd45ad163ed82f8cb50ef6e76d7016ee4c918e56489f1a43162` with stable DSO SHA `0e1750c7ef509096201c849116d28ae385bd429ee914e4adebbd8960f0284041`. All three fixtures load the extension and stop at `host.storage_owner_transport / E05 / UNAVAILABLE`; no world admission follows.

A fresh current-tree Debug DSO build returned rc0 with SHA `187850139bba4c1e124af25629ef4901a958ef9188e86b7b126e99e6b50fd96b` (receipt `work/dso-diagnostic-rebuild-current-20261030-r5/RECEIPT.json`). Re-running the three fixed Metal controls with that DSO and the same editor again returned rc2 at the identical E05 boundary; the new semantic-bundle error labels were therefore not reached (receipt `work/u01-vfx-diagnostic-r2-20261030/RECEIPT.json`).

The remaining production seam is external owner input. E05 transport/account data, E06 issuer, and R01/D completion/readback/disposal authority are absent. The native substrate work continues independently and does not promote these negative runtime results to issuer, completion, Ready, or G1 authority.

CI run `40930940598` completed with one failure in the Linux doubles GCC sanitizer job; the lld repair itself was not reached because the earlier tracked receipt files under ignored `work/` failed the static gitignore check. The workflow repair is integrated as `f6b0fc8152`, and the receipt-only follow-up is removed from the tracked tree by `a0574e6941` while retained locally. The workflow declares `linker: lld`, installs `lld` when selected, and keeps `use_static_cpp=no`; YAML parsing and `git diff --check` pass. A new public run is still required before claiming CI green.

The completed public attempt then exposed a separate test contract failure in `tests/core/object/test_message_queue.cpp`: the capacity helper left a Message-sized tail, so the next operation returned `OK` instead of `ERR_OUT_OF_MEMORY`. The test-only repair is committed as `6237e0b4f5`; its patched translation unit compiles locally with rc0, and a new public run is required. The red CI receipt is local at `work/ci-message-queue-fix-20261030/RECEIPT.md`.

The repaired tests-enabled macOS editor rebuilt with rc0 at SHA `dbb1f9df221a3bcff799ef26eea11f1ab68804f1f7a8444b92af9913f95d530f`; `*[CallQueue]*` passed 6 cases / 92 assertions. This local validation does not change the public CI status and does not certify native/GPU/Ready behavior.

E06/E09 selector audit: no matching doctest tags exist in the current source; zero-match runs are diagnostic only and do not prove issuer or recorder coverage.

The same tests-enabled editor passed `[DeviceExit][SceneReady]*` with 2 cases / 52 assertions, preserving malformed/guarded scene-ready refusal behavior.

The same tests-enabled editor passed full `[Qualification]*`: 91 cases / 1829 assertions, rc0; expected ERR_UNAUTHORIZED negative diagnostics remain visible.

The same tests-enabled editor passed full `*device_exit*`: 133 cases / 2619 assertions, rc0; expected fail-closed callback diagnostics remain visible.

The same tests-enabled editor passed full `*[QualificationNative]*`: 67 cases / 302 assertions, rc0.

The native/renderer source set is now integrated as commit `84c9b2ee7d`; the tests-enabled binary and selector receipts are pinned to that source state.

The same tests-enabled editor passed full `*[K03]*`: 44 cases / 1153 assertions, rc0.

The same tests-enabled editor passed full `*[R02]*`: 9 cases / 115 assertions, rc0.

The same tests-enabled editor passed full `*[RenderingDevice]*`: 12 cases / 214 assertions, rc0.
