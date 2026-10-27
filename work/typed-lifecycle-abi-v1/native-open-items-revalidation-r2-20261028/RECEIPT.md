# Current native open-items revalidation R2

Date: 2026-10-28

Godot HEAD: `75d99c2e62fa8a7b1c8653118168e5aa8075b6e7`
ANGE HEAD: `8e2ed56fc1bcc6ae24567f238d994648f4b133bd`

The current census confirms that the native lane has real bounded implementation evidence but is not fully landed. The following item-scoped seams remain open:

1. `require_main_rd_qualification()` remains a binding API and test surface; no production `register_types` caller was found. The production flag remains default-false unless the original owner sets it.
2. `StageExecutor::start_native_qualification` and `ParticleProgramLoader::request_qualification` remain implemented definitions/delegation without a normal production preparation/record actor issuing the sequence.
3. Graph handles and command-buffer mapping exist, but the evidence still does not establish the full checked append → native issuer → completion → same-request exact-range readback chain.
4. Post-SCENE SceneTree/Engine wrapper enrollment and final instance-binding disposal remain required for the G1 close gate.

No fallback issuer, synthetic association, operation key, completion, readback, Ready/Verified state, or G1 relaxation was introduced. The exact commands and outputs are retained in `COMMAND_OUTPUT.json`; source identities are in `SOURCE_HASHES.txt`.

This revalidation constrains only the corresponding native acceptance rows. Independent ANGE particle source/cache/provider work remains active.
