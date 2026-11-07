# Fixed Scene 4a0c intake

Read-only source intake. No adoption, source overwrite, build, GPU run, commit, or runtime acceptance.

- Fixed manifest commit: `4a0c26723fccee9f90bc75930b27635e006f48c7` (base `f62766d37c561d59c0b7949cd73fa2003015db77`). All 17 payload source files independently match manifest bytes, SHA256 and Git blob SHA1. See MANIFEST_HASH_VERIFICATION.json; this proves payload integrity, not binary provenance or production Complete.
- `HeightFrameResources` cpp/h are substantively identical to local. Only filename-padding in license lines differs. Do not overwrite them.
- Fixed sink adds an owner-Ref constructor taking `Ref<HeightFrameResources>`, retaining that exact owner and deriving RD from it. Fixed `HeightFrame` depends on this constructor (`height_frame.cpp:37-39`). Local sink lacks that overload; this is a real adoption dependency.
- Fixed `begin()` revalidates retained quoted resources: same RD, quoted verified, observe success, LIVE depth/framebuffer and exact requested framebuffer equals quoted framebuffer. This protects the owner-Ref constructor path through the renderer's plain `sink.begin()`. Local `begin_quoted_target()` validates its explicit target path, but plain `begin()` has no retained-owner validation; restoring fixed constructor requires restoring this check, not only adding an overload.
- Preserve local generic-native additions when adopting: explicit `prepare_native_range`, exact Ref, private RD call scopes/per-record callbacks, TESTS access, terminal-request state and finish/unwind close paths. Fixed sink does not contain these and cannot replace local wholesale. Source integrity does not prove worker/hidden/transfer/cleanup or producer/reader native contracts.
- Local `height_frame.cpp/h` are absent. Fixed payload includes only cpp (manifest has no height_frame.h). Do not synthesize a header or claim this packet is a complete compilable HeightFrame port. Complete original fixed header/caller intake is still required for that owner slice.

# Host issuer clarification

Available local `host_qualification_service.cpp` inventory contains 32 files with the named definitions. None of the extracted `HostControlService::require_request_issuer` or `request_qualification` bodies contains an UNAVAILABLE literal. Both active-gpu-publication/ANGE and sprite-instance192-shader-consumption snapshots explicitly use require_loader then loader->require_qualification_parent; request_qualification checks issuer, loader, main device, then delegates. `require_main_device` can legitimately return UNAVAILABLE when no main pair exists; downstream store can reject missing qualification context. Those dynamic failures are not unconditional issuer stubs.

The parent supplied authoritative A fixed commit `2c918ff6a2ffd03030c9dad72be4eca40e8042f3`, host_qualification_service.cpp:207-212, as current require_loader→loader.require_qualification_parent implementation. This receipt does not independently establish that commit from these copied files; it records the observed bodies and hashes. Old Scene lines 39-63 cannot establish current A missing-issuer work. No duplicate issuer implementation is warranted from this intake.
