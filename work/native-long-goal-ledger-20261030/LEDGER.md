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

The 2026-10-30 native source closure audit confirms `src/` and `tests/` are clean at Godot HEAD `3608b6d1a95057836a987f23ea00cbc1907e6d4f`; no additional uncommitted native source/test patch remains. This is source-state evidence only and does not close E05/U08, public CI, SceneReady, native/GPU, completion/readback, Ready, or G1.

An initial independent ANGE Release observation produced a signed artifact (SHA `c7c52d79361ec11d52c464cf91f4a5276d8de49fb28e5deb7ab1783c1ede5e21`; codesign rc0), but its original build emitted no return code and a transient follow-up returned rc2 because the external worktree briefly lacked `godot-cpp/SConstruct`; this observation was not treated as a green build claim.

The external ANGE template_release recheck later returned explicit rc0 and codesign rc0, with artifact SHA `3907b32111a59024c7f850cdd2a1272d5ab6a8c1a5ae4c2f3db89a38224d14dc`; receipt is `work/e05-e09-release-build-recheck-20261030/RESULT.md`. Build-before/after HEAD drifted during the run, so this closes only the observed Release process lane, not a frozen commit-only artifact; owner, native/GPU qualification, completion/readback, Ready and G1 remain unqualified.

The shared ANGE line now integrates the typed E05 owner snapshot consumer and MainRD overload through commits `4246a6f4f`, `a1330f538`, `7e402ea37`, and `93684f435`. Owner Debug/UBSan tests pass 2 cases / 9 assertions and MainRD regression passes 14 cases / 1718 assertions. The consumer remains read-only; `HostAttachmentInstaller::require_storage_owner_inputs()` still returns `UNAVAILABLE` for `host.storage_owner_transport`, so no production issuer/native/GPU/Ready claim follows.

Godot commit `5629a017f1bfa4f3562adaa792e984ddc19fd053` repairs the test-only P01 collector sequencing after intentional poison/reject operations. The tests-enabled editor build returned rc0 at SHA `cd07b546dc34220448cf8580405c6ef8745758bccbfd51146a50439613d0bd57`; P01 passed 91 cases / 9612 assertions and CallQueue passed 6 / 92. Production filename audit found zero `p01_probe`/`probe` paths outside test/work/build; detached external godot-cpp remains outside the Godot build input, so no ABI/GPU/Ready claim follows.

The shared ANGE host runner exposed and then closed an E05 consumer ABI bug: `validate_storage_owner_budget()` incorrectly compared the budget prefix size to `sizeof(GDExtensionQualificationBudgetV1)`. Commits `9f9b4e4ad` and `c007389a2` align it with the frozen `GDExtensionDeviceExitAbi` prefix. Current `test-host-storage` builds rc0 and `[host_storage_boundary]` passes 7 cases / 352 assertions; `host.storage_owner_transport / E05 / UNAVAILABLE` remains the intentional production gate.

The current Godot source line also carries an independently verified native texture-import cleanup: `f4b839b4a0` changes `texture_rd_initialize` to a bool contract across RD/GLES3/dummy backends, with fresh module-on Debug rc0 and RenderingDevice 12/214, Qualification 102/1896, K03 44/1153 and R01 16/293. Static contract coverage is green; live backend-fault injection is unavailable in the current harness. Receipts: `work/native-texture-import-failure-20261030/` and `work/native-texture-import-regression-20261030/`.

The current mesh external-surface audit passes B4/K05 4/101 plus the same RenderingDevice/Qualification/K03/R01 controls, but `mesh_add_external_surface` has no production caller. The missing owner/generation/packet issuer remains upstream; no synthetic caller or GPU/Ready claim was added. Receipt: `work/mesh-external-surface-audit-20261030/`.

ANGE C06 remains fail-closed at the original C04 resolver: the C06 finalizer and view preserve the sole authority chain, while `resolve_gpu_linked` lacks the authenticated linked payload/revocation resolver and no production GPU writer consumes the finalized mapping. Receipt: `work/c06-finalized-physical-mapping-consumer-gap-20261030/`.

An intermediate validation detected that the old full-selector ledger was pinned to editor SHA `fb8693bd...` while the current executable had SHA `37743639...`; that drift is now resolved by the current-editor selector recheck below. No old result was reused across the boundary.

The current tests-enabled editor recheck bound to SHA `37743639...` passes every ledger selector at rc0: K03/P2 25/386, QualificationNative Metal 2/51, QualificationNative Vulkan 65/251, MetalCompletion 6/386, ParticlesReadback 2/5, E05 18/417, QualificationRt 6/458, TypedABI 12/356, SceneReady 2/52, Qualification 102/1896, device_exit 133/2619, QualificationNative 67/302, K03 44/1153, R02 9/115, RenderingDevice 12/214. The old `fb8693...` binary boundary is retired for current selector claims; exact runtime pair evidence remains separately pinned and E05-negative.

The committed C04 gap audit confirms `resolve_gpu_linked()` remains the sole fail-closed gate because the declared link/restore producers, `GpuLinkResult`, retained schedule/units/physical map and revocation state are absent from tracked source. The existing C06 finalizer is correct; no fabricated lease or map was added. Receipt: ANGE `work/c04-linked-resolver-gap-20261030/`.

The committed Godot evidence matrix now binds the canonical pair to editor SHA `37743639...`, ANGE DSO SHA `38df3a53...`, and three rc2 E05 owner-boundary probes, while retaining each historical production-fix and audit row under its own binary hashes. Matrix: `work/evidence-matrix-20261030/`; no row claims native owner, completion/readback, Ready or GPU Ready.

The committed evidence matrix validation (`work/evidence-matrix-20261030/MATRIX_VALIDATION.json`, rc0) confirms current editor SHA `37743639...`, DSO SHA `38df3a53...`, all 12 evidence package paths, and the three expected rc2 E05-boundary probes. Validation is identity/reachability evidence only and does not promote owner, completion, readback, Ready or GPU claims.

ANGE E17/E18 scene consumer audit is also current: pure sphere/heightfield geometry passes 2/565 and 1/401, lifecycle boundary passes 1/6, but no production caller joins scene geometry to the authenticated lifecycle/output owner. The missing input is the original scene geometry issuer/registry projection carrying scene revision, lifecycle generation/dense range, material identity and query/readback provenance; no second scene authority was added. Receipt: ANGE `work/e17-e18-scene-consumer-gap-20261030/`.
