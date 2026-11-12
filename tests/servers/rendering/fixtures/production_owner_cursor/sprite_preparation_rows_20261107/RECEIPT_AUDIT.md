# Receipt audit (read-only verification)

Existing final build and CPU runs are audited, not rerun. No GPU claim. `RAW_OUTPUT_MANIFEST.json` fixes raw XML, stdout/stderr, final build log/result and host identity by bytes/SHA256.

- CPU_RESULT.json: committed receipt matches work bytes = True
- FINAL_BUILD_RESULT.json: committed receipt matches work bytes = True
- FINAL_BUILD_SOURCE.json: committed receipt matches work bytes = True
- FINAL_HOST.json: committed receipt matches work bytes = True
- source core/extension/gdextension_production_output.cpp: bytes/SHA match = True
- source servers/rendering/renderer_rd/storage_rd/external_output_owner.cpp: bytes/SHA match = True
- source servers/rendering/renderer_rd/storage_rd/external_output_owner.h: bytes/SHA match = True
- source servers/rendering/renderer_rd/storage_rd/external_output_packet.h: bytes/SHA match = True
- source servers/rendering/renderer_rd/storage_rd/particles_storage_retire.cpp: bytes/SHA match = True
- source tests/core/extension/test_production_request.cpp: bytes/SHA match = True
- source tests/servers/rendering/test_external_output_custody_bridge.h: bytes/SHA match = True
- source tests/servers/rendering/test_external_output_owner_state.cpp: bytes/SHA match = True
- source tests/servers/rendering/test_record_source.cpp: bytes/SHA match = True
- FOCUSED: assertions={'successes': '4604', 'failures': '0'}; cases={'successes': '18', 'failures': '0', 'skipped': '2241'}
- COMBINED: assertions={'successes': '90846', 'failures': '0'}; cases={'successes': '217', 'failures': '0', 'skipped': '2042'}
- current host binary matches frozen receipt bytes/SHA = True

The final source is the actual base acd6310a0b + CI portability fixes + preparation row edits, frozen by FINAL_BUILD_SOURCE.json. It is not a clean post-receipt-commit rebuild. FINAL_BUILD.log shows an incremental build, not a full rebuild; wrapper exit 0/time12.02s is in FINAL_BUILD_RESULT.json, while SCons reports10.04s. COMBINED_OUTPUT.txt preserves three expected pipeline.driver_id refusal diagnostics; XML still reports zero failures. CPU host is headless and is not GPU validation.

README.md is the implementation agent's historical handoff: “No build, CPU test or GPU run was performed by this agent” and “parent owns both” refer to that agent, not the parent execution subsequently recorded in CPU_RESULT/RECEIPT. Its SOURCE_FREEZE.json and patch references point to uncommitted work artifacts, not files in the seven-file committed receipt. Minimal suggested clarification (not applied): title as “Original implementation-agent handoff”; add one opening sentence linking RECEIPT.md/FINAL_BUILD_SOURCE.json/CPU_RESULT.json as the later parent validation and naming RAW_OUTPUT_MANIFEST.json as raw-evidence index.

Recent history:
```
55f00ae609433ca03f1a425dd3f7a906d922e21f Record Sprite preparation row validation
15d8884529b63efe04a99d178821391ea2660089 Reserve native Sprite preparation rows per output slot
e5de6a06dc49af6807fd974a128d390413fd5213 Fix cross-platform test warning initializers
acd6310a0b84ff80ea90cec106f4a0e9ec7a855c Cover native Scene unregister on a foreign thread
4e847586229b43b64fa67d4a6b56b603dcff156c Repair Scene117 CI evidence and formatting gates
```
